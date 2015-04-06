#include <windows.h>
#include <wininet.h>
#include <shlobj.h>

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

typedef unsigned nwp_t;

void error(const char* msg){
	FILE* f=fopen("error.log","a");
	if(f){
		time_t t=time(0);
		fputs(asctime(localtime(&t)),f);
		fputs(" : ",f);
		fputs(msg,f);
		fputs("\n\n",f);
		fclose(f);
	}
}

nwp_t random(nwp_t hi){
	static std::mt19937 mt{(unsigned)std::chrono::system_clock::to_time_t(
		std::chrono::high_resolution_clock::now()
	)};
	return std::uniform_int_distribution<nwp_t>(0,hi)(mt);
}

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
		strcmp(dot,".png") && strcmp(dot,".jpg") && strcmp(dot,".jpeg") &&
		strcmp(dot,".bmp") && strcmp(dot,".jng")
	);
}

void traverse(std::string path,std::vector<std::string>& wallpapers){
	const char mask[]="\\*";
	
	WIN32_FIND_DATA fdata;
	HANDLE search=FindFirstFile((path+mask).c_str(),&fdata);
	if(search==INVALID_HANDLE_VALUE){
		throw std::runtime_error("Couldn't start directory search");
	}
	
	struct SObj{
		HANDLE search;
		std::string name;
	};
	std::stack<SObj> dirs;
	
	for(;;){
		if(!(fdata.cFileName[0]=='.' ||
				fdata.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN
		)){
			if(fdata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
				dirs.push({search,path});
				path+="\\";
				path+=fdata.cFileName;
				search=FindFirstFile((path+mask).c_str(),&fdata);
				if(search==INVALID_HANDLE_VALUE){
					SObj sob=dirs.top();
					search=sob.search;
					path=sob.name;
					dirs.pop();
				}
				continue;
			}
			
			if(is_image(fdata.cFileName)){
				wallpapers.push_back(path+"\\"+fdata.cFileName);
			}
		}
		
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
					path=sob.name;
					dirs.pop();
				}
			}
			else{
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
	GetFullPathName(path,MAX_PATH,buf,NULL);
	wchar_t wbuf[MAX_PATH];
	mbstowcs(wbuf,buf,MAX_PATH);
	
	CoInitializeEx(0,COINIT_APARTMENTTHREADED);
    IActiveDesktop* desktop;
    CoCreateInstance(
		CLSID_ActiveDesktop,NULL,CLSCTX_INPROC_SERVER,
		IID_IActiveDesktop,(void**)&desktop
	);
    WALLPAPEROPT wOption={sizeof(WALLPAPEROPT),WPSTYLE_STRETCH};
    desktop->SetWallpaper(wbuf,0);
    desktop->SetWallpaperOptions(&wOption,0);
    desktop->ApplyChanges(AD_APPLY_ALL);
    desktop->Release();
    CoUninitialize();
	/*
	SystemParametersInfo(
		SPI_SETDESKWALLPAPER,0,(PVOID)buf,SPIF_UPDATEINIFILE
	);
	*/
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

//Load a file meant to save the status of the shuffling
nwp_t read_status(std::vector<nwp_t>& indices,nwp_t wallpapers){
	FILE* f=fopen("status","rb");
	indices.clear();
	
	if(f){
		nwp_t i,data;
		fread(&i,sizeof(nwp_t),1,f);
		
		while(fread(&data,sizeof(nwp_t),1,f)){
			indices.push_back(data);
		}
		
		fclose(f);
		
		return i;
	}
	
	while(wallpapers--){
		indices.push_back(wallpapers);
	}
	
	std::random_shuffle(indices.begin(),indices.end(),random);
	
	fclose(f);
	
	write_status(0,indices);
	
	return 0;
}

//update the indices and shuffling status file given the
// wallpaper and check lengths
nwp_t update(nwp_t i,std::vector<nwp_t>& ind,nwp_t wall,nwp_t check){
	nwp_t num=wall-check;
	//files have been added
	if(num>0){
		//add new indices to access the files
		for(nwp_t x=check;x<check-num;++x){
			nwp_t v=random(wall);
			
			//make sure adding an index before
			// the current index won't affect it
			if(v<=i){
				i+=1;
			}
			ind.insert(ind.begin()+v,x);
		}
	}
	//files have been removed
	else if(num<0){
		for(nwp_t x=wall-num;x<wall;++x){
			auto v=std::find(ind.begin(),ind.end(),x)-ind.begin();
			
			//make sure removing an index before
			// the current index won't affect it
			if(v<i){
				i-=1;
			}
			ind.erase(ind.begin()+v);
		}
	}
	
	write_status(i,ind);
	
	return i;
}

#include <iostream>
using namespace std;

int main(int argc,char* argv[]){
	unsigned delay;
	const char* root;
	
	//load command line arguments, handling defaults
	if(argc>2){
		stringstream(argv[1])>>delay;
		root=argv[2];
	}
	else if(argc>1){
		stringstream(argv[1])>>delay;
		//root defaults to the current directory
		root=".";
	}
	else{
		//1 minute default
		delay=60;
		root=".";
	}
	
	//SendMessageTimeout(FindWindow("Progman",NULL),0x52c,0,0,0,500,NULL);
	
	std::vector<nwp_t> indices;
	std::vector<std::string> wallpapers;
	traverse(root,wallpapers);
	nwp_t i=read_status(indices,wallpapers.size());
	
	try{
		for(;;){
			while(i<indices.size()){
				//start a timer just in case the operations take too long
				auto start=std::chrono::steady_clock::now();
				for(;;){
					const char* wall=wallpapers[indices[i]].c_str();
					if(exists(wall)){
						set_wallpaper(wall);
						break;
					}
					
					std::vector<std::string> check;
					traverse(root,check);
					i=update(i,indices,wallpapers.size(),check.size());
					wallpapers=check;
				}
				
				write_status(++i,indices);
				
				//sleep by the delay minus however much time has
				// already passed
				std::this_thread::sleep_for(
					std::chrono::seconds(delay)+
					start-std::chrono::steady_clock::now()
				);
			}
			
			//check for any new wallpapers
			std::vector<std::string> check;
			traverse(root,check);
			
			//update the indices and shuffling status
			i=update(i,indices,wallpapers.size(),check.size());
			wallpapers.swap(check);
		}
	}
	catch(std::runtime_error& e){
		error(e.what());
		return 1;
	}
}
