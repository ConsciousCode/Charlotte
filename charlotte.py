#!/usr/bin/python
import os
import sys
import random
import time
import pickle

#Windows
if sys.platform.startswith("win"):
	import ctypes
	import pythoncom
	from win32com.shell import shell,shellcon
	
	#import functions used to activate wallpaper fading
	SendMessageTimeout=ctypes.windll.user32.SendMessageTimeoutW
	FindWindow=ctypes.windll.user32.FindWindowW
	
	i=ctypes.c_int()
	#Sends a signal to Progman to fade when wallpaper changes
	SendMessageTimeout(FindWindow("Progman",None),0x52c,None,None,0,500,ctypes.byref(i))
	
	def set_wallpaper(path):
		#use the ActiveDesktop interface
		desktop=pythoncom.CoCreateInstance(shell.CLSID_ActiveDesktop,None,pythoncom.CLSCTX_INPROC_SERVER,shell.IID_IActiveDesktop)
		desktop.SetWallpaper(os.path.abspath(path),0)
		desktop.ApplyChanges(shellcon.AD_APPLY_ALL)
else:
	raise NotImplementedError("Environment is not supported")

#load command line arguments, handling defaults
if len(sys.argv)>2:
	delay=float(sys.argv[1])
	root=sys.argv[2]
elif len(sys.argv)>1:
	delay=float(sys.argv[1])
	#root defaults to the current directory
	root='.'
else:
	#1 minute default
	delay=60.0
	root='.'

#supported image extensions
imgexts=['.jpg','.jpeg','.png','.bmp']

#return a list of all image paths in a given directory
def traverse(p):
	wallpapers=list()
	
	for path,dirs,files in os.walk(p):
		wallpapers.extend([os.path.join(path,f) for f in files if os.path.splitext(f)[1] in imgexts])
	return wallpapers

#load the pickled status from the status file
def read_status():
	#loop to retry 
	while True:
		try:
			if os.path.isfile("status"):
				indices,i=pickle.load(open("status","rb"))
				i+=1
			else:
				#file's missing, produce new indices
				indices=[x for x in range(len(wallpapers))]
				random.shuffle(indices)
				i=0
			break
		except (IOError,EOFError,pickle.PickleError):
			os.remove("status")
			
			open("error.log","a").write("{}: Failed to load status dump\n\t{}".format(time.asctime(),'\t'.join(traceback.format_exception(type(e),e,e.__traceback__))))
	
	return indices,i

#save the status as a pickle
def write_status(indices,i):
	try:
		pickle.dump((indices,i),open("status","wb"))
	except IOError as e:
		open("error.log","a").write("{}: Failed to write status dump\n\t{}".format(time.asctime(),'\t'.join(traceback.format_exception(type(e),e,e.__traceback__))))

#update the indices and shuffling status file given the
#wallpaper and check lengths
def update(i,ind,wall,check):
	num=wall-check
	if num>0:
		for x in range(wall-num,wall):
			v=indices.index(x)
			#adjust the index so modification doesn't
			#affect iteration
			if v<i:
				i+=1
			del indices[v]
	elif num<0:
		for x in range(check,check-num):
			v=random.randrange(i,wall)
			
			if v<=i:
				i-=1
			indices.insert(v,x)
	
	write_status(indices,i)

wallpapers=traverse(root)

#load where it left off
indices,i=read_status()

#indefinitely loop
while True:
	#iterate over the indices
	#(the indices are shuffled rather than the wallpapers
	# to support adding and removing wallpapers to the
	# directory without complicated insertion and iteration
	# algorithms)
	while i<len(indices):
		while True:
			try:
				wall=wallpapers[indices[i]]
				#check for the edge case that the user
				#removed the file since the last traversal
				if not os.path.isfile(wall):
					raise IndexError()
				set_wallpaper(wall)
				break
			except IndexError:
				check=traverse(root)
				update(i,indices,len(wallpapers),len(check))
				wallpapers=check
		
		#start a timer just in case the costly operations
		#take too long
		start=time.perf_counter()
		
		#check for any new wallpapers
		check=traverse(root)
		
		#update the indices and shuffling status
		update(i,indices,len(wallpapers),len(check))
		wallpapers=check
		
		i+=1
		#sleep by the delay minus however much time has
		#already passed
		time.sleep(delay+start-time.perf_counter())
	
	i=0
	#shuffle the indices instead of the wallpapers
	random.shuffle(indices)
	#dump the fresh status
	write_status(indices,0)