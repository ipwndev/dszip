# Home of dsZip #

dsZip is a handy utility to compress and decompress .nds files directly on the Nintendo DS. This way it is possible to store many more homebrew applications (or whatever floats your boat!) on a memory card than possible when using normal files, and decompress them only when you really want to use them.

The project is based on the GPL'd gzip source code (version 1.24). It uses the devKitPro build tool and libfat to access the memory device.

## Project status ##

**BIG BOLD NOTE: this is pre-alpha quality code. It might work, or not. It might fail in silent and subtle ways. It might destroy your console, or it might eat your data. Use it at your own risk, and do not complain if something goes wrong. You've been warned.**

**dsZip is provided without any warranty. Really.**


### What works ###

  * The gzip decompressor should be OK (tested with different compressed file sizes, up to 128 MB).
  * The user interface is very basilar but allows directory navigation and file selection.
  * Tested on a DS Lite with a [R4](https://code.google.com/p/dszip/source/detail?r=4) adapter.

### What does not work ###

  * Error reporting is minimal (hint: result = 0 means everything is fine).
  * NO checks on the output file name apart from extension stripping (could overwrite something important).
  * Compression does not work yet.
  * Archives different from .gz are not supported (even if gzip can normally handle them).
  * No check is done for available space on the device.

### What I'm planning to do ###

  * Support different kinds of archives (at the very least .zip).
  * Support for compression.
  * More than a file in a archive (e.g. compress saves with the .nds).
  * Testing, testing, testing.
  * Better GUI.

## Usage instructions ##

**PLEASE note again: this is PRE-ALPHA quality code. Experiment/use at your own risk. Not suitable for those who do not know what they are doing.**

The following assumes you know at least a bit how to run homebrews on the Nintendo DS (TM) console. I won't explain that here, for now. Maybe when the code is in a better shape, just to be sure no newbie gets hurt.

### Pre-compiled binary ###
  1. Download the latest release from the [Downloads section](http://code.google.com/p/dszip/downloads/list).
  1. Unzip the binary.
  1. From now on, follow the instructions for patching the binary you can find below.

### Compile from source ###
  1. Install the devKitPro toolchain.
  1. Check out the code from the SVN repository.
  1. A normal make command should suffice. If everything goes well you should have a trunk.nds file in the trunk directory of the repo.
  1. Please rename the .nds file to something more meaningful :-)

### Patch the binary ###

Both the downloaded version and the compiled binary have to be patched in order to work with different DS devices.

You probably will have to obtain the appropriate .dldi file for your device and run the dlditool executable on the newly downloaded/generated .nds file. Some newer devices with up-to-date firmwares might allow the executable to work even without patching. Do what you would do for a normal libfat binary.

Pleas don't ask any questions about patching the binary or stuff like that. If you do not  know how to do that and can't figure it out with other resources you find on the 'net you probably do not want to run the current dsZip version anyhow.

### Uploading the binary to the DS ###

Do as you would do for any other homebrew application.

### Running dsZip ###

When dsZip is run it displays a list of files and directories present in the current directory (that is, the one it has been launched from). Only .gz files are currently listed, as those are the only one you can decompress for now.

The currently selected file/directory is identified with a `->` sign. You can change the currently selected entry by pressing UP/DOWN on the D-pad. The list will scroll, of course. Note that the touchscreen is not used for now.

If you press A, an action is taken. If the currently selected entry is a directory, you will move into that directory. Pressing B or activating the `..` entry (that should be present in any directory) goes back to the parent folder.

If it is a file, dsZip will try to decompress the file. It will list the gzip'd file name, the output file name and then a progress indicator (for every compressed block that has been decompressed a `.` is printed). Decompression is done when the message asking you to reboot your DS is printed on screen. Please wait for that message before resetting or power cycling your DS. If it seems to be stuck it might be because you are decompressing a very large file. Just wait a bit. An uncompressed 128MB file can take up to 4-5 minutes.

Just before the reboot message an exit code is printed. 0 is fine, everything else represents failure.

## Legal notes ##

This project is not affiliated with nor approved, endorsed or sponsored by the Nintendo corporation. They probably do not even know that this project exists.

The Revolution [R4](https://code.google.com/p/dszip/source/detail?r=4) card name is (might be?) a trademark of the r4ds.com team.

The Nintendo DS name is a trademark of the Nintendo corporation. It is used here only to design the target platform for the applications. I do not wish to exploit it for commercial purposes, etc. etc. etc.

Please do not ask about using dsZip with commercial games. It might work with them, or not (in the end it is just a lossless compressor/decompressor). Please do not ask where to get them. I do not endorse piracy.