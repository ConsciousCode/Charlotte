Charlotte
=========
Named after Charlotte Perkins Gilman for her story "The Yellow Wallpaper".

Designed for two reasons:
 * 100+ wallpapers are a pain to organize, and the shuffler that comes with Windows doesn't traverse the file hierarchy
 * Even if the default shuffler followed the hierarchy, it uses a purely random selection algorithm, thus the same wallpaper was often selected soon after its last selection, giving the illusion of repetition. Charlotte guarantees that files will never repeat until every other file has been seen.

Python
------
### Running
charlotte.py takes the following command line arguments:
> python charlotte.py [delay [root]]
> 	delay	The delay in seconds (real, defaults to 60)
> 	root	The root folder for getting wallpapers (defaults to .)

### Suggested usage
Make a batch/bash file which calls charlotte.py with pythonw with your preferred arguments and add it to your startup. If you have some other wallpaper slideshow program/setting, be sure to disable it beforehand.

### How to support more OSes
At the top, immediately after the import statements, there's an if-elif-else block which checks for OSes. Just implement the change_wallpaper function, and that should cover all portability issues.

### How it works
 * At startup either loads the pickled shuffling status or builds a randomized index list
   - An index list is used to simplify the code for handling the case where the user adds a new wallpaper in between shuffles (if it were just a shuffled list of paths, complicated iteration and insertion/removal code would have to be used)
 * Changes the wallpaper via a set_wallpaper function based on the host OS
 * Periodically traverses the root directory for new wallpapers

C++
---
After some time it was noted that the Python version might not be as fast as it ought to be, and at startup there was a brief moment where the console popped up before being destroyed. So, a C++ version was made.

### Running
charlotte.exe takes the following command line arguments:
> charlotte [delay [root]]
> 	delay	The delay in seconds (integer, defaults to 60)
> 	root	The root folder for getting wallpapers (defaults to .)

### Suggested usage
Simply run compile.bat and add charlotte.exe to your startup; gcc is given the -mwindows option, which disables the creation of a console. As with the Python version, be sure to disable any other wallpaper shuffling software.

### How to support more OSes
At the top of charlotte.cpp is a couple of preprocessor #if-#elif-#else chains. The first determines the environment OS and includes OS-specific files (which must be included first to avoid overriding std defines). The second one is used to implement the 4 OS-specific functions: traverse (which builds a list of all the image files), set_wallpaper, init, and deinit. Implementing these within their own block will satisfy portability.

### How it works
 * At startup either loads the stored shuffling status (entirely 32-bit unsigned integers, first being the last index used, the rest a list of the randomized indices) or builds a randomized index list
   - An index list is used to simplify the code for handling the case where the user adds a new wallpaper in between shuffles (if it were just a shuffled list of paths, complicated iteration and insertion/removal code would have to be used)
 * Changes the wallpaper via a set_wallpaper function based on the host OS
 * Periodically traverses the root directory for new wallpapers