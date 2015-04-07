#define WINDOWS 1
#if defined(_WIN32) || defined(_WIN64)
	#define OS WINDOWS
#else
	#error Unknown OS environment
#endif

#if OS==WINDOWS
	#include <windows.h>
	#include <wininet.h>
	#include <shlobj.h>
#endif

#include <cstdio>
#include <ctime>
#include <climits>

#include <cstring>

#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <string>
#include <vector>
#include <stack>

#include <thread>
#include <chrono>

//Used to decouple the underlying index type from logic
typedef unsigned nwp_t;

//OS-specific functions (these aren't declared for flexibility)
// void traverse(std::string,std::vector<std::string>&);
// void set_wallpaper(const char* path);
// void init();
// void deinit();

//Forward declarations for the OS-specific functions
nwp_t random(nwp_t);
void error(const char*);
bool exists(const char*);
bool is_image(const char*);
void write_status(nwp_t,const std::vector<nwp_t>&);
nwp_t read_status(std::vector<nwp_t>&,nwp_t);
nwp_t update(const char*,nwp_t,std::vector<nwp_t>&,std::vector<std::string>&);

#if OS==WINDOWS
	void traverse(std::string path,std::vector<std::string>& wallpapers){
		WIN32_FIND_DATA fdata;
		HANDLE search=FindFirstFile((path+"\\*").c_str(),&fdata);
		if(search==INVALID_HANDLE_VALUE){
			throw std::runtime_error("Couldn't start directory search");
		}
		
		//This makes it easy to keep track of the searching state
		struct SObj{
			//The search handle so directory searching can be resumed 
			// mid-search.
			HANDLE search;
			//The path of the directory tree up to this point.
			std::string path;
		};
		std::stack<SObj> dirs;
		
		//Complicated breaking logic, use an "infinite" loop
		for(;;){
			//Filenames that start with . are considered "hidden" in Unix
			// environments, and this also ignores the . and .. directories.
			if(!(fdata.cFileName[0]=='.' ||
					fdata.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN
			)){
				if(fdata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
					dirs.push({search,path});
					path+="\\";
					path+=fdata.cFileName;
					search=FindFirstFile((path+"\\*").c_str(),&fdata);
					//Don't even care, the directory's probably protected or
					// something
					if(search==INVALID_HANDLE_VALUE){
						SObj sob=dirs.top();
						search=sob.search;
						path=sob.path;
						dirs.pop();
					}
					continue;
				}
				
				if(is_image(fdata.cFileName)){
					wallpapers.push_back(path+"\\"+fdata.cFileName);
				}
			}
			
			//This is a loop so the next file isn't a repeat of an older one,
			// which messes up the rest of the logic.
			while(!FindNextFile(search,&fdata)){
				if(GetLastError()==ERROR_NO_MORE_FILES){
					if(dirs.size()==0){
						FindClose(search);
						return;
					}
					else{
						FindClose(search);
						SObj sob=dirs.top();
						search=sob.search;
						path=sob.path;
						dirs.pop();
					}
				}
				else{
					//The handles haven't been RAII'd, so we have to free
					// them first
					while(dirs.size()){
						FindClose(dirs.top().search);
						dirs.pop();
					}
					
					throw std::runtime_error("Couldn't find the next file");
				}
			}
		}
	}

	void set_wallpaper(const char* path){
		char buf[MAX_PATH];
		//Have to get the full pathname for Windows to know what we're
		// talking about
		GetFullPathName(path,MAX_PATH,buf,NULL);
		
		//Convert the path to a wchar_t string
		wchar_t wbuf[MAX_PATH];
		mbstowcs(wbuf,buf,MAX_PATH);
		
		//Some kind of fancy ActiveDesktop magic; The Python version does 
		// almost exactly the same thing.
		//You can change the desktop with SystemParametersInfo, but this
		// is the only way to get it to fade in between images.
		CoInitializeEx(0,COINIT_APARTMENTTHREADED);
		IActiveDesktop* desktop;
		CoCreateInstance(
			CLSID_ActiveDesktop,NULL,CLSCTX_INPROC_SERVER,
			IID_IActiveDesktop,(void**)&desktop
		);
		
		//Note WPSTYLE_STRETCH here - it may be desirable to add more
		// options to the program to switch this out.
		WALLPAPEROPT wOption={sizeof(WALLPAPEROPT),WPSTYLE_STRETCH};
		desktop->SetWallpaper(wbuf,0);
		desktop->SetWallpaperOptions(&wOption,0);
		desktop->ApplyChanges(AD_APPLY_ALL);
		
		desktop->Release();
		CoUninitialize();
	}
	
	void init(){
		//Must be sent at least once per boot to enable fade transitions
		//This tells Windows to enable ActiveDesktop, which may be disabled
		SendMessageTimeout(FindWindow("Progman",NULL),0x52c,0,0,0,500,NULL);
	}
	
	void deinit(){}
#endif

//RNG
std::mt19937 rng;

nwp_t random(nwp_t hi){
	return std::uniform_int_distribution<nwp_t>{0,hi}(rng);
}

//There might be faster ways to implement this using OS-specific code
//Alternatively, this could be merged with set_wallpaper
bool exists(const char* fn){
	FILE* f=fopen(fn,"rb");
	if(f){
		fclose(f);
		return true;
	}
	return false;
}

bool is_image(const char* fn){
	const char* dot=strrchr(fn,'.');
	return dot && !(
		//This list isn't exhaustive, but close enough for wallpaper extensions
		//These are roughly in order of expected usage frequency
		strcmp(dot,".png") && strcmp(dot,".jpg") && strcmp(dot,".jpeg") &&
		strcmp(dot,".bmp") && strcmp(dot,".jng")
	);
}

//Write the current status to the status file
void write_status(nwp_t i,const std::vector<nwp_t>& indices){
	FILE* f=fopen("status","wb");
	if(!f){
		throw new std::runtime_error("Cannot open status file");
	}
	
	fwrite(&i,sizeof(nwp_t),1,f);
	for(nwp_t x: indices){
		fwrite(&x,sizeof(nwp_t),1,f);
	}
	fclose(f);
}

//Write just the position to the beginning of the file
void write_pos(nwp_t i){
	FILE* f=fopen("status","r+b");
	if(!f){
		throw new std::runtime_error("Cannot open status file");
	}
	fseek(f,0,SEEK_SET);
	
	fwrite(&i,sizeof(nwp_t),1,f);
	fclose(f);
}

//Load a file meant to save the status of the shuffling
nwp_t read_status(std::vector<nwp_t>& indices,nwp_t wallpapers){
	FILE* f=fopen("status","rb");
	
	//Sanity check
	indices.clear();
	
	if(f){
		nwp_t i,data;
		//Make sure the file doesn't just exist with no data
		if(fread(&i,sizeof(nwp_t),1,f)==sizeof(nwp_t)){
			while(fread(&data,sizeof(nwp_t),1,f)){
				indices.push_back(data);
			}
			
			fclose(f);
			
			//Make sure there's data to read
			if(indices.size()){
				return i;
			}
		}
	}
	
	//Build any list which enumerates all possible indices
	while(wallpapers--){
		indices.push_back(wallpapers);
	}
	
	//Shuffle the list - this'll be iterated over incrementally to
	// guarantee no repetitions
	std::shuffle(indices.begin(),indices.end(),rng);
	
	fclose(f);
	
	write_status(0,indices);
	
	return 0;
}

//Update the indices and shuffling status file given the
// wallpaper and check lengths
nwp_t update(
		const char* root,nwp_t i,
		std::vector<nwp_t>& ind,
		std::vector<std::string>& wallpapers
){
	std::vector<std::string> check;
	traverse(root,check);
	nwp_t w=wallpapers.size(),c=check.size();
	//Swapping is probably faster than copying
	//wallpapers=check;
	wallpapers.swap(check);
	
	//Files have been removed
	if(w>c){
		for(nwp_t x=c;x<w;++x){
			auto v=std::find(ind.begin(),ind.end(),x)-ind.begin();
			
			//Make sure removing an index before the current index won't 
			// affect it
			if(v<i){
				i-=1;
			}
			ind.erase(ind.begin()+v);
		}
	}
	//Files have been added
	else if(c>w){
		//Add new indices to access the files
		for(nwp_t x=w;x<c;++x){
			//Insertion point should be random up to the max index
			nwp_t v=random(x);
			
			//Make sure adding an index before the current index won't
			// affect it
			if(v<=i){
				i+=1;
			}
			ind.insert(ind.begin()+v,x);
		}
	}
	
	//If wall == check, there may be some changes to the file hierarchy
	// but there's no need to update the indices
	
	i%=w;
	write_status(i,ind);
	
	return i;
}

int main(int argc,char* argv[]){
	try{
		std::chrono::seconds delay;
		const char* root;
		
		//Load command line arguments, handling defaults
		if(argc>2){
			delay=std::chrono::seconds{atoi(argv[1])};
			root=argv[2];
		}
		else if(argc>1){
			delay=std::chrono::seconds{atoi(argv[1])};
			//Root defaults to the current directory
			root=".";
		}
		else{
			//1 minute default
			delay=std::chrono::seconds(60);
			root=".";
		}
		
		rng.seed((unsigned)time(0));
		//MinGW tries to open /dev/urandom, which crashes the program
		//rng.seed(std::random_device{}());
		
		//Run any OS-specific initialization
		init();
		
		std::vector<nwp_t> indices;
		std::vector<std::string> wallpapers;
		traverse(root,wallpapers);
		nwp_t i=read_status(indices,wallpapers.size());
		
		for(;;){
			while(i<indices.size()){
				//start a timer just in case the operations take too long
				auto start=std::chrono::steady_clock::now();
				//Loop until the wallpaper is set, put the failsafe at 5
				for(int fails=0;;++fails){
					//Make sure we don't create an infinite loop because
					// of some unforeseeable edge case
					if(fails>5){
						throw std::runtime_error("Unable to set wallpaper");
					}
					
					const char* wall=wallpapers[indices[i]].c_str();
					if(exists(wall)){
						set_wallpaper(wall);
						break;
					}
					
					i=update(root,i,indices,wallpapers);
				}
				
				write_pos(i);
				++i;
				
				//Sleep by the delay minus however much time has already passed
				std::this_thread::sleep_for(
					delay-(std::chrono::steady_clock::now()-start)
				);
			}
			
			//Check for any new wallpapers
			i=update(root,i,indices,wallpapers);
		}
	}
	catch(const std::exception& e){
		//Run any necessary deinitialization of resources
		deinit();
		
		//Log the error to the error log
		FILE* f=fopen("error.log","a");
		if(f){
			time_t t=time(0);
			fprintf(f,"%s%s\n\n",asctime(localtime(&t)),e.what());
			fclose(f);
			//Signal an error to the environment
			return 1;
		}
		
		//At least we can alert the user that something's wrong
		throw e;
	}
	//Just let the program crash if it gets to this point
	//catch(...){}
}
