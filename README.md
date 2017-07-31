# esp82xx

Useful ESP8266/ESP8285 C Environment.
Includes libraries and some basic functionality such as a Web-GUI, flashing firmware and changing the UI over network as well as basic GPIO functions.

You can use it as a template for your own ESP8266/ESP8285 projects.
Just include it as sub-module in derivate projects.

NOTE: If you are starting from another environment or from the factory, you should execute ```make erase initdefault``` this will set the flash up in a way compatible with the newer ESP NONOS SDK 2.x's.

**Contributors,** please read the notes closely before you start (e.g. [Branches](#branches) and [Include Binaries](#include-binaries)).
Make changes in the `dev` branch!

- [List of projects using esp82xx](#list-of-projects-using-esp82xx)
- [Usage](#usage)
    - [Requirements](#requirements)
    - [Start a new Project](#start-a-new-project)
    - [Specify SDK](#specify-sdk)
    - [Burn Firmware](#burn-firmware)
    - [Connect to your Module](#connect-to-your-module)
    - [Commands](#commands)
    - [Troubleshooting](#troubleshooting)
- [Notes](#notes)
    - [Create File Structure by hand](#create-file-structure)
    - [Branches](#branches)
    - [Submodule Updates](#submodule-updates)
    - [Include Binaries](#include-binaries)
- [ToDo](#todo)

## List of projects using esp82xx

 - [esp82XX-basic](https://github.com/con-f-use/esp82XX-basic)
 - [Colorchord](https://github.com/cnlohr/colorchord)
 - [MAGFest Swag](https://youtu.be/DbjlStyMmaY?t=8m) [Badges 2017](https://github.com/cnlohr/swadges2017)
 - [esp8266ws2812i2c](https://github.com/cnlohr/esp8266ws2812i2s)
 - [espusb](https://github.com/cnlohr/espusb)
 - Migration of others in progress

## Usage

### Requirements

*Most of this is written having a Debian-like Linux distribution in mind.
You will need to use your imagination, if you want to build on other platforms.
People have done it on OSX and Win 7/8.
Look at open and closed issues and internet resources on the SDK such as the
 [esp-forum](http://www.esp8266.com/search.php?search_id=active_topic) to find help.*

You will need the following:

 - [Espressif](https://espressif.com) toolchain for the esp82xx-series chips
 - [Libusb](http://libusb.info) 1.0 (`sudo apt-get install libusb-1.0-0-dev`)
 - GNUMake
 - GNU Compiler Collection and build essentials
 - Possible more

We recommend the excellent [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk) by @pfalcon.
It downloads and installs the Espressif toolchain.
Here is a shell script to [download and build](https://gist.github.com/con-f-use/d086ca941c2c80fbde6d8996b8a50761) a version known to work.
You should read and understand the script, before running it.
It will take some time and GBs of disk space to build the toolchain.

Some versions of the SDK are somewhat problematic, e.g. with SDK versions greater than 1.5.2, Espressif changed the IRAM management, so some projects began to hit size restrictions and would not compile.
Also some SDKs use different initial data (the flash has to have some SDK-related settings stored that are not userspace and aren't flashed along with the firmware).
For that reason, the Makefile is set up to use a [customized version](https://github.com/cnlohr/esp_nonos_sdk) of the SDK and ships with proven initial data.

#### Specify SDK

There are many ways to [let Make know where your SDK is](https://github.com/cnlohr/esp82xx/issues/19#issuecomment-241756095) located.
You can edit `DEFAULT_SDK` in `./user.cfg` to reflect your specific SDK path or **even better** define a shell variable.
The latter is done with

    # Add this in ~/.bashrc or similar
    export ESP_ROOT=/path/to/sdk

in your `.bashrc`, `.profile` or whatever is used by your shell. This way, the change will be persistent, even if you start many new esp82xx projects.

You can also pass the location as an argument to make:

    make all ESP_ROOT=path/to/sdk

### Start a new Project

Starting a new project based on esp82xx is pretty easy:

    mkdir my_new_esp_project
    cd my_new_esp_project
    git clone --recursive https://github.com/CNLohr/esp82xx
    cp esp82xx/Makefile.example Makefile
    make project

Replace the last line by the line below, if you also want to initialize the folder as a new git repo and upload it to a remote location:

    make gitproject GIT_ORIGIN=https://github.com/YOUR_USER/YOUR_NEW_REPO.git

After the above commands, provided you have [specified the SDK](#specify-sdk), the basic file structure should be in place.
Most files will be symbolic links against files in `./esp82xx/`.
**Do not** edit these files or anything in `./esp82xx/`.
You should rather copy the files to the top-level directories and edit the copies where necessary.
I.e. if you want add a line of CSS that changes the font in the WebUI, copy `./esp82xx/web/page/intex.html` to `./web/page/` overwriting the symbolic link.
Then make your edits.

Edit `user.cfg` in the top level to specify things like the location of the Espressif toolchain (see [Requirements](#requirements)).
The file  `user.cfg` specifies the most important configuration variables.

### Burn Firmware

If you did everything correctly, flashing your esp should work now.

Just connect it to an USB to serial adapter that uses 3.3V (**you will fry your ESP with voltages higher than 3.3 V**) and place it in programming mode.

Optionally you can flash the default inital data now with:

    make initdefault

We talk about it at the end of the [Requirements](#requirements) section.
It should not be necessary to do that, provided you use fairly recent esp-modules.
However, using different `make init3v3` gives you the possibility to monitor the 3.3 V supply voltage rail with the ADC instead of using it via the ADC pin.

Whether you flash the inital data or not, your next step will be to burn the firmware.
Just go into programming mode and run:

    make burn
    make burnweb  # programming mode here too

Your ESP is good to go now.
It should create its own WiFi Access Point called `ESPXXXX` or similar, where `XXXX` stands for some unique string.
From now on you can configure and burn new firmware/page data over the web interface in your browser (when connected to the esp's network or the ESP is connected to yours).
There are make targets to burn firmware and page data as well:

    make netburn IP=192.168.4.1  # default IP, change to whatever your ESP is set to
    make netweb IP=192.168.4.1

To find out the IP, see below.

### Connect to your Module

The ESP will print its connection info, including its current IP to the serial interface after reset/power-on.

You can [connect to the ESP](http://esp82xx.local) in your browser:

    http://esp82xx.local

There is also a make-target called `getips` that scans for ESP modules and lists their IPs.
`make getips` is basically a port-scan, that uses external tools you might have to install and takes long (especially if no ESP is connected).

The default IP of the ESP, when it operates as it's own access point, is **192.168.4.1**.
When connected to an existing WiFi Network, it will ask your DHCP-Server for an IP.
Most WiFi routers have an option in their Web-GUI to list all IPs, that their DHCP has given out.
You could find out your ESP's IP this way.

For general troubleshooting hints, see [esptools troubleshooting page](https://github.com/themadinventor/esptool#troubleshooting).
It is excellent!

### Commands

Most features of the WebUI can be used over network by sending an ASCII string to the flashed ESP.
This way you can evoke a feature from a program, script or the shell (i.e. using `nc` a.k.a netcat).

The command string starts with a letter signifying the command or family of commands.
In case of a command family, the second letter usually defines the command.
After that comes the first argument.
Subsequent arguments after the first are usually followed by tab-characters.
Most commands will also send a response packet containing information or error codes.
The character(s) that specify the command(-family) are not case sensitive, i.e. `E1234` and `e1234` will be interpreted identically.

Here is an example to get a freshly flashed ESP to connect to a WiFi network named `MyWiFi`:
```
echo -ne "W1\tMyWiFi\t1234\ta1:b2:c3:d4:e5:f6\t1" | nc 192.168.4.1 7878  -u -w 1 | hexdump -C

```
The access point has the mac `a1:b2:c3:d4:e5:f6` and password `1234`.
The program `hexdump` is used to receive and display the ESP's answer.

The commands are:

| Family | Command | Description |
| :----: | :-----: | ----------- |
| **B**  |         | **Browse commands** |
|        | q       | Probe |
|        | s       | Service name |
|        | l       | List IP, service, device name and description |
|        | r       | Response |
|        |         |  |
| **G**  |         | **GPIO commands** |
|        | 0       | Turn pin with the same number as the first argument on |
|        | 1       | Turn pin with the same number as the first argument off |
|        | i       | Make pin numbered same as the first argument an input |
|        | f       | Toggle pin number specified by the first argument |
|        | g       | Get status of pin numbered same as the first argument |
|        | s       | Get outputmask and rmask as base-ten numbers |
|        |         |  |
| **E**  | .       | **Echo command string with all arguments (for test purposes)** |
|        |         |  |
| **I**  |         | **Get info** |
|        | b       | Restart system |
|        | s       | Save CS-settings |
|        | l       | Load CS-settings 1 |
|        | r       | Load CS-settings 2 |
|        | f       | Start finding devices or return list of found devices |
|        | n       | Device name |
|        | d       | Device description |
|        | .       | General info: IP, device name, description, servie name, free heap |
|        |         |  |
| **W**  |         | **Wifi commands** |
|        | 0       | Have the ESP an AP. Arguments: AP name, password, mac, channel |
|        | 1       | Connect to existing network AP. Same arguments as above |
|        | i       | Get info on current Wifi settings |
|        | x       | Get RSSI (if applicable) and current IP |
|        | s       | Scan for WiFi stations |
|        | r       | Return results of scan |
|        |         |  |
| **F**  |         | **Flashing commands** |
|        | e       | Erase sector |
|        | b       | Erase block |
|        | m       | Execute flash rewriter |
|        | w       | Write given number of bytes to flash (binary) |
|        | x       | Write given number (second argument) of hexadecimal bytes (third argument) to position in (first argument) in flash |
|        | r       | Read a number of bytes (second argument) from a sector (first argument) flash |
| **C**  |         | ** Custom commands ** |
|        | C       | Respond with 'CC' |
|        | E       | Echo arguments to UART |
|        | ...     | Your commands could go here |
|        |         |  |


A dot `.` stands for an omitted character, i.e. nothing.
It is not included in the command string.


For more information on these commands, please conuslt the source code.
The commands are implemented in `./esp82xx/fwsrc/commonservices.c`.
You can add your own commands in `./user/custom_commands.c`.

## Troubleshooting

### Caught in Reset loop

Most of endless resets are caused by one of the following:
 - Power supply too weak (the esp can draw 300mA)
 - Not enough decoupling capacitance (a combination of 100uF and  100nF work)
 - Wonky connection (worn out, loose or dirty contacts
 - Initial flash data faulty (see [Specify SDK](#specify-sdk)), in which case this might help:
 ```
 make erase
 make initdefault #init3v3
 ```

### Gibberish Serial Data right after Boot

If you get weird data at the start of your serial communication with the esp, don't forsake!
The boot ROM writes a log to the UART with the unusual timing of 74880 baud.
It's normal.

### Useful Links

 - [esp8266 Getting Started Guide](https://espressif.com/en/support/explore/get-started/esp8266/getting-started-guide)
 - [esp-forum](http://www.esp8266.com/search.php?search_id=active_topic)
 - [NodeMCU docs](https://nodemcu.readthedocs.io/en/master/en/flash/)

## Notes

This section should mostly concern developers and contributors to this project.
We try to keep the generally interesting stuff on top.

### Create File Structure

You can create the file structure of a basic program by hand or based on another project, instead of running `make project`.
To do that first, check out a project that uses esp82xx

    git clone --recursive https://github.com/con-f-use/esp82XX-basic

or create your own

    git init project_name
    cd project_name
    git submodule add https://github.com/cnlohr/esp82xx.git
    cp esp82xx/user.cfg.example user.cfg
    cp esp82xx/Makefile.example Makefile
    mkdir -p web/page user
    ln -s ../esp82xx/web/Makefile web/
    # ... link or copy more files depending on how much you want to change ...

After that, you can push it a freshly created remote repository with the usual git commands:

    git init .
    git add .
    git git remote add origin https://github.com/YOUR_USER/YOUR_NEW_REPO.git
    git commit -m 'Initial commit'
    git push

The basic Makefile for the firmware is `./esp82xx/main.mf`.
Most things can be achieved by including it in your top-level Makefile and changing some make variables.

    include esp82xx/main.mf

    SRCS += more_sources.c # Add your project specific sources

### Branches

If you make small incremental changes and/or experimental ones, push to the `dev` branch rather than to origin/master:

    git push origin dev

You can merge or squash-merge them into `master` once they have been tested and enough changes accumulate.

    # ... Make changes in dev ...
    # ... test them and be sure they should go into master ...
    git checkout master
    git merge --squash dev
    git commit

It might be good to create feature branches to develop individual features and merge them into `dev`.

### Submodule Updates

Cope with submodule changes in top-level project:

 - You made changes in the submodule and want to push them:

    ```
    cd esp82xx   # is the submodules here
    # Make changes in esp82xx
    git commit -m 'Your Message'
    git push     # pushes to the sumodule remote
    ```
    Make sure you're in the 'dev' branch.
    You can check that with `git branch`.
    If you're not, make sure to `git checkout dev` first, BEFORE you make your changes.
    You can also use `git push origin dev` to push just the dev branch.

    When you're ready to push, first make sure 'master' is up to date with
    `git push origin dev:master`.
    Then to push to master, use `git push origin dev:master`

 - Bump esp82xx version in the main project root folder:

    ```
    cd esp82xx
    git pull
    cd ..
    git add esp82xx
    git commit -m 'Bumped submodule version'
    git push
    ```

 - Make sure you are using the master branch of your submodules, when you're about to merge a dev version of top-level projects.
 I.e. just before you merge, checkout the master branch of submodules, so that when you merge, a Master-branch top-level projects uses the master-branch of all its submodules and not subodule dev-braches.

 - You can clone the dev-branch directly:

    ```
    git clone --recursive -b dev https://github.com/cnlohr/esp82xx.git && cd esp82xx
    ```

   If you forgot and switched to `dev` via `git checkout dev`, you might not have the proper submodules. In that case run `git submodule update --init`

### Include Binaries

You should **not** include binaries in the project repository itself.
There is a make target that builds the binaries and creates a `.zip` file. Included that in the release.
To make a release, just tag a commit with `git tag -a 'v1.3.3.7' -m 'Your release caption'` and push the tag with `git push --tags`.

After that, the github web-interface will allow you to make a release out of the new tag and include the binary file.
To make the zip file invoke `make projectname-version-binaries.tgz` (Tab-autocompletion is your friend).

## ToDo

 - Include libraries for usb, ws2812s and ethernet
 - Expand the "Requirements" section
