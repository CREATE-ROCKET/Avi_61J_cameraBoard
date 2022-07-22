from ast import arg
import glob
import subprocess
import os
import platform
import sys


def main(args):
    # args[0]: python, args[1]:inputDir, args[2]:outputDir
    outputDir = ''
    if(len(args) == 2):
        outputDir = 'mp4'
    elif(len(args) == 3):
        outputDir = args[2]
    else:
        sys.exit(
            '[error] parameter error \'getMP4fromh264 (inputDir) (outputDir)\'')
    os.makedirs(outputDir, exist_ok=True)

    if(platform.system() == 'Windows'):
        sys.exit('[error] Your system is Windows. You should do in WSL with gpac.')

    files = glob.glob(args[1]+'/*.h264')
    for file in files:
        print(file)
        subprocess.run(['MP4Box', '-fps', '30', '-add',
                        file, file.replace(args[1], outputDir).replace('.h264', '.mp4')])
    sys.exit(
        '\033[32m'+'[success] All the videos have been converted.'+'\033[0m')


if __name__ == "__main__":
    args = sys.argv
    main(args)
