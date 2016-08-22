# esp82xx

Useful ESP8266 C Environment. Intended to be included as sub-modules in derivate projects.

## Usage

First, check out a project that uses esp82xx

    git clone --recursive https://github.com/con-f-use/esp82XX-basic

or create your own

    git init project_name
    cd project_name
    git submodule add git@github.com:cnlohr/esp82xx.git
    cp esp28xx/user.cfg.example user.cfg
    cp esp28xx/Makefile.example Makefile
    mkdir -p web/page user
    ln -s esp28xx/web/Makefile web/
    # ... link or copy more files depending on how much you want to change ...

After you have the basic file structure in place, you should edit `user.cfg` in the top level.
**Do not** edit files in `./esp82xx`.
You should rather copy them to top-level directories and edit/include the copies where necessary.
The basic Makefile for the firmware is `./esp82xx/main.mf`.
Most things can be achieved by including it and changing some make variables like this:

    include esp82xx/main.mf

    SRCS += more_sources.c # Add your project specific sources

The file  `user.cfg` specifies the most important configuration variables.
Most notably the location of the Espressif SDK for building the firmware.
You will need a working copy of that.
We recommend the excellent [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk) by @pfalcon to download and install all necessary tools.
Here is a shell script to [download and build](https://gist.github.com/con-f-use/d086ca941c2c80fbde6d8996b8a50761) a version known to work.
Some versions of the SDK are somewhat problematic, e.g. with SDK versions greater than 1.5.2, Espressif changed the IRAM management, so some projects began to hit size restrictions and would not compile.

You can edit `DEFAULT_SDK` in user.cfg to reflect your specific SDK path or **even better** define a shell variable `export ESP_ROOT=/path/to/sdk` in your `.bashrc`, `.profile` or what-ever is used in your shell.
You can also pass the location as an argument to make:

    make all ESP_ROOT=path/to/sdk

If you did everything correctly, flashing your esp should work.
Just connect it to an USB to serial adaptor that uses 3.3V (you will fry your ESP with higer voltages) and place it in programming mode.
Then you can run

    make burn
    make burnweb

and your ESP is good to go.
It should create its own WiFi Access Point called `ESPXXXX` or similar, where `XXXX` is some number.
From now on you can configure and burn new firmware/page data over the web interface in your browser (when connected to the esp's network or it is connected to yours).
There are make targets to burn firmware and page data as well:

    make netburn IP=192.168.1.4  # default IP, change to whatever your ESP is set to
    make netweb IP=192.168.1.4

You can [connect to the ESP](http://cn8266.local) in your browser

    http://cn8266.local

There is also a make-target called `getips` and the ESP will print its connection info to the serial interface.


## List of projects using esp82xx

 - [esp82XX-basic](https://github.com/con-f-use/esp82XX-basic)
 - [Colorchord](https://github.com/cnlohr/colorchord)
 - [esp8266ws2812i2c](https://github.com/cnlohr/esp8266ws2812i2s)
 - [espusb](https://github.com/cnlohr/espusb)
 - Migration of others in progress

## Notes

This section should mostly concern developers and contributors to this project.

#### Branches

If you make small incremental changes and/or experimental ones, push to the `dev` branch rahter than to origin/master:

    git push origin dev

You can merge or squash-merge them into `master` once they have been tested and enough changes accumulate.

    # ... Make changes in dev ...
    # ... test them and be sure they should go into master ...
    git checkout master
    git merge --squash dev
    git commit

It might be good to create feature brances to develop individual features and merge them to dev and then from there to master or a hotfix branch for important quick-fixes.

#### Submodule Updates

Cope with submodules in top-level projects updates:

 - Changes in the submodule:

    ```
    cd esp82xx
    # Make changes
    git commit -m 'Your Message'
    git push
    ```
    p.s. make sure you're in the 'dev' branch.  You can check that with ```git branch``` If you're not, make sure to ```git checkout dev``` first, BEFORE you make your changes. 

    When committing, commit to dev branch.  When you're ready to push, first make sure 'master' is up to date... ```git push origin dev:master``` then to push to master, use this: ```git push origin dev:master```

 - Then bump the version in the main project root folder:

    ```
    cd esp82xx
    git pull
    cd ..
    git add esp82xx
    git commit -m 'Bumped submodule version'
    git push
    ```

#### Include Binaries

You should **not** include binaries in the project repository itself.
There is a make target that builds the binaries and creates a `.zip` file. Included that in the release.
To make a release, just tag a commit with `git tag -a 'v1.3.3.7' -m 'Your release caption'` and push the tag with `git push --tags`.

After that, the github web-interface will allow you to make a release out of the new tag and include the binary file.
To make the zip file invoke `make projectname-version-binaries.tgz` (Tab-autocomplete is your friend).

### To do

 - Include libraries for usb, ws2812s and ethernet

