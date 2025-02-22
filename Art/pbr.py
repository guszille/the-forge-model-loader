# Copyright (c) 2017-2025 The Forge Interactive Inc.
#
# This file is part of The-Forge
# (see https://github.com/ConfettiFX/The-Forge).
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.


"""
Helper script for managing the PBR folder and texture files.

Usage:
$ pbr.py initialize $MATERIAL_FOLDER_NAME
  - creates a new folder with the given $MATERIAL_FOLDER_NAME
  - creates the 2K and 1K folders inside $MATERIAL_FOLDER_NAME

$ pbr.py check
  - Goes into every material folder and checks if the naming scheme is applied or not.
    Prints a list of folders that have incorrectly named texture files.
    
    
$ pbr.py fixname $MATERIAL_FOLDER_NAME
  - goes into 2K and 1K folders in $MATERIAL_FOLDER_NAME and
     renames the texture files with the following scheme:
        Albedo.png
        AO.png
        Normal.png
        Height.png
        Metallic.png
        Roughness.png
  - The command will rename the files meeting the following name 
    scheme, which is the scheme used by the website we get the
    assets from:
        $MATERIAL_FOLDER_NAME_Base_Color.png
        $MATERIAL_FOLDER_NAME_AO.png
        $MATERIAL_FOLDER_NAME_Normal.png
        ...

$ pbr.py fixall
  - TODO:
"""

import sys
import os
import subprocess
import threading
import time
import platform


# LIST OF COMMANDS
# -----------------------------
CMD_INIT   = "init"
CMD_CHECK  = "check"
CMD_FIX    = "fixname"
CMD_FIXALL = "fixall"
# -----------------------------

# HELPERS ----------------------------------------------------------------------------
def PrintHelp():
    print(' ')
    print('PBR Folder Helper for managing texture file names and folder structure.')
    print(' ')
    print('Usage:')
    print('    pbr.py [command] [args]')
    print(' ')
    print('Available Commands:')
    print('    ', CMD_INIT)
    print('    ', CMD_CHECK)
    print('    ', CMD_FIX)
    print('    ', CMD_FIXALL)
    print(' ')

def CheckCommand(cmd):
    if (  not cmd == CMD_INIT
      and not cmd == CMD_CHECK
      and not cmd == CMD_FIX
      and not cmd == CMD_FIXALL
    ):
        return False
    return True



# COMMANDS ---------------------------------------------------------------------------
def ExecCmdInit():
    if len(sys.argv) < 3:
        print('init: missing argument: please provide the folder name to initialize.')
        return

    folder_name = sys.argv[2]
    
    # create the folder provided in the argument
    try:
        os.mkdir(folder_name)
    except OSError:
        print('Could not create directory: ', folder_name)

    # change dir and create subfolders
    os.chdir(folder_name)
    try:
        os.mkdir("2K")
    except OSError:
        print('Could not create directory: 2K')
    try:
        os.mkdir("1K")
    except OSError:
        print('Could not create directory: 1K')

    return

def ExecCmdCheck():
    print('ExecCmdCheck')
    return


def ExecCmdFixname():
    texture_types = ["Roughness", "Height", "Metallic", "Normal", "AO", "Base_Color"]

    # expect user to provide which folder to fix
    if len(sys.argv) < 3:
        print('fixname: missing argument: please provide the folder name to apply name fixes.')
        return

    folder_name = sys.argv[2]

    if not os.path.isdir(folder_name):
        print('fixname: cannot find folder: ', folder_name)
        return

    os.chdir(folder_name)
    subfolders = os.listdir('.')

    for subfolder in subfolders:
        os.chdir(subfolder)

        # find the files with incorrect naming convention
        files = [f for f in os.listdir('.') if os.path.isfile(os.path.join('.', f))]
        files_to_fix = []
        for ffile in files:
            for tex_type in texture_types:
                if tex_type in ffile and not ffile == (tex_type + ".png"):
                    files_to_fix.append(ffile)

        # fix the file names
        if len(files_to_fix) > 0:
            for ffile in files_to_fix:
                for tex_type in texture_types:
                    if tex_type in ffile:
                        name_prev = ffile
                        name_curr = ffile
                        if tex_type == "Base_Color":
                            name_curr = "Albedo.png"
                            os.rename(ffile, name_curr)
                        else:
                            name_curr = tex_type + ".png"
                            os.rename(ffile, name_curr)

                        print('Rename \'', name_prev, '\'\t-> \'', name_curr, '\'')
                        break

        os.chdir('../')

    return
	

def GetTextureList(directory, filename):
	alpha_textures = []
	
	if os.path.exists(directory + filename):
		f = open(directory + filename, "r")
		lines = f.readlines()
		for line in lines:
			line = line.rstrip('\n')
			alpha_textures.append(line)
		f.close()
	
	return alpha_textures

def FindInArray(find_array, find):
	for f in find_array:
		if f.lower() in find.lower():
			return 1
	return 0
	
def TexConvertFunction(texconv, filename, options):
	dirname = (os.path.dirname(os.path.realpath(filename)))
	command = '"%s" "%s" %s -o "%s"'%(
        texconv,
        filename,
		options,
        dirname,
    )
	
	DEVNULL = open(os.devnull, 'wb')
	p = subprocess.call(command, stdout=DEVNULL, stderr=DEVNULL)
	return


def CompressFunction(compress, filename, options):
	output = os.path.splitext(filename)[0] + ".dds"
	command = '"%s" %s "%s" "%s"'%(
        compress,
		options,
        filename,
        output,
    )
	
	DEVNULL = open(os.devnull, 'wb')
	p = subprocess.call(command, stdout=DEVNULL, stderr=DEVNULL)
	return

def PVRCompressFunction(compress, filename, options):
	output = os.path.splitext(filename)[0] + ".pvr"
	command = '%s -i "%s" -o "%s" %s'%(
        compress,
        filename,
        output,
		options,
    )
	DEVNULL = open(os.devnull, 'wb')
	p = subprocess.call(command, stdout=DEVNULL, stderr=DEVNULL, shell=True)
#	p = subprocess.call(command, shell=True)
	return

def KTXCompressFunction(compress, filename, options):
	output = os.path.splitext(filename)[0] + ".ktx"
	command = '"%s" -o "%s" %s "%s"'%(
        compress,
        output,
        options,
		filename,
    )
	# print(command)
	DEVNULL = open(os.devnull, 'wb')
	p = subprocess.call(command, stdout=DEVNULL, stderr=DEVNULL, shell=True)
	# p = subprocess.call(command)
	return
	

def ExecCmdFixAll():
	print('ExecCmdFixAll')
	
	start = time.time()
	
	texconv = "tools/texconv.exe"
	nvcompress = "tools/nvcompress.exe"
	amdcompress = "CompressonatorCLI.exe"
	pvrcompress = "tools/img2ktx.exe"
	ktxcompress = "tools/img2ktx.exe"
	
	relative_dir = ""
	# expect user to provide which folder to fix
	if len(sys.argv) < 3:
		print('fixname: missing argument: using directory of script.')
	else:
		relative_dir = sys.argv[2]

	rootdir = os.path.join(os.path.dirname(os.path.realpath(__file__)), relative_dir)
	print(rootdir)
	
	max_open_files = 128

	extensions = [ ".png", ".jpg", "jpeg", ".hdr" ]
	tasks = []
	files_to_fix = []
	temp_files = []
	files_open = 0
	alpha_textures = GetTextureList(rootdir, "alpha_textures.txt")
	uncompressed_textures = GetTextureList(rootdir, "uncompressed_textures.txt")
	small_textures = GetTextureList(rootdir, "small_textures.txt")
	
	print("Convert all PNG, JPG to TGA, HDR to DDS")
	
	for subdir, dirs, files in os.walk(rootdir):
		for file in files:
			ext = os.path.splitext(file)[-1].lower()
			if ext in extensions:
				filename = (os.path.join(subdir, file))
				files_to_fix.append(filename)
	
	for file in files_to_fix:
		options = "-y"
		ext = os.path.splitext(file)[-1].lower()
		if "hdr" in ext:
			options = options + " -ft dds -f BC6H_UF16"
		else:
			if FindInArray(uncompressed_textures, file) == 1:
				options = options + " -ft dds -f R8G8B8A8_UNORM"
			else:
				options = options + " -ft tga"
				if "metallic" in file.lower() or "roughness" in file.lower():
					options = options + " -f R8_UNORM"
				temp_files.append(os.path.splitext(file)[0] + ".tga")

		thread_args = (texconv, file, options)
		t = threading.Thread(target=TexConvertFunction, args=thread_args)
		t.start()
		tasks.append(t)
		files_open = files_open + 1
		if files_open > max_open_files:
			for thread in tasks:
				thread.join()
			files_open = 0
			tasks = []
		
	for thread in tasks:
		thread.join()
		
	print("Convert all textures to desktop compressed format")
	
	extensions = [ ".tga" ]
	tasks = []
	files_to_fix = []
	files_open = 0

	for subdir, dirs, files in os.walk(rootdir):
		for file in files:
			ext = os.path.splitext(file)[-1].lower()
			if ext in extensions:
				filename = (os.path.join(subdir, file))
				files_to_fix.append(filename)

	for file in files_to_fix:
		options = ""
		ext = os.path.splitext(file)[-1].lower()
		compress = nvcompress
		alpha = FindInArray(alpha_textures, file)
		
		if alpha == 1:
			options = options + " -color -alpha -rgb"
		else:
			if "normal" in file.lower():
				options = options + " -normal -bc5"
			elif "tga" in ext and ("metallic" in file.lower() or "roughness" in file.lower()):
				options = options + " -color -bc4"
			else:
				options = options + " -color -bc1"
		
		# CompressFunction(compress=compress, filename=file, options=options)
		thread_args = (compress, file, options)
		t = threading.Thread(target=CompressFunction, args=thread_args)
		t.start()
		tasks.append(t)
		files_open = files_open + 1
		if files_open > max_open_files:
			for thread in tasks:
				thread.join()
			files_open = 0
			tasks = []
			
	for thread in tasks:
		thread.join()
		
	for file in temp_files:
		os.remove(os.path.abspath(file))
		
	print("Convert all textures to mobile compressed format")
		
	extensions = [ ".tga", ".png", ".jpg", "jpeg" ]
	tasks = []
	files_to_fix = []
	files_open = 0
	
	for subdir, dirs, files in os.walk(rootdir):
		for file in files:
			ext = os.path.splitext(file)[-1].lower()
			if FindInArray(uncompressed_textures, file) == 0 and ext in extensions:
				filename = (os.path.join(subdir, file))
				files_to_fix.append(filename)

	for file in files_to_fix:
		options = "-m"
		ext = os.path.splitext(file)[-1].lower()
		compress = ktxcompress
		alpha = FindInArray(alpha_textures, file)
		quality = "fast"
		small = FindInArray(small_textures, file)
		if small == 1:
			astc = "ASTC4x4"
		else:
			astc = "ASTC8x8"
		
		if alpha == 1:
			options = options + " -f ASTC4x4 -flags \"-alphablend -%s\""%(quality)
		else:
			if "normal" in file.lower():
				options = options + " -f ASTC4x4 -flags \"-normal_psnr -%s\""%(quality)
			elif "metallic" in file.lower() or "roughness" in file.lower():
				options = options + " -f %s -flags \"-ch 1 0 0 0 -esw r000 -dsw r000 -%s\""%(astc, quality)
			else:
				options = options + " -f %s -flags \"-%s\""%(astc, quality)
		
		# KTXCompressFunction(compress=compress, filename=file, options=options)
		thread_args = (compress, file, options)
		t = threading.Thread(target=KTXCompressFunction, args=thread_args)
		t.start()
		tasks.append(t)
		files_open = files_open + 1
		if files_open > max_open_files:
			for thread in tasks:
				thread.join()
			files_open = 0
			tasks = []
			
	for thread in tasks:
		thread.join()
		
	end = time.time()
	print(end - start)

	return


# ENTRY POINT --------------------------------------------------------------------------
def Main():
    # arg check
    if len(sys.argv) < 2:
        PrintHelp()
        return

    # command check
    cmd = sys.argv[1]
    if not CheckCommand(cmd):
        print('Incorrect command: ', cmd)
        PrintHelp()
        return
    
    # exec commands
    if cmd == CMD_INIT:
        ExecCmdInit()
    elif cmd == CMD_CHECK:
        ExecCmdCheck()
    elif cmd == CMD_FIX:
        ExecCmdFixname()
    elif cmd == CMD_FIXALL:
        ExecCmdFixAll()
    


# print '# args: ', len(sys.argv)
# print ':: ', str(sys.argv)
if __name__ == "__main__":
    Main()

