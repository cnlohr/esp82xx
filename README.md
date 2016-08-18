# esp82xx

Useful ESP8266 C Environment. Intended to be included as sub-modules in derivate projects.

## List of projects using esp82xx

 - [Colorchord](https://github.com/cnlohr/colorchord)

### Notes

 - Outline make a new project:
```
git init project_name
cd project_name
git submodule add git@github.com:cnlohr/esp82xx.git
cp esp28xx/user.cfg.example user.cfg
cp esp28xx/Makefile.example Makefile
mkdir -p web/page
mkdir user
ln -s esp28xx/web/Makefile web/
```
Then link, edit and copy to your heart's content. See [esp82XX-basic](https://github.com/con-f-use/esp82XX-basic) for a minimal example.

 - You should **not** include binaries in the project repository itself.
 There is a make target that builds the binaries and creates a zip file that can be included with the release.
 To make a release, just tag a commit with `git tag -a 'v1.3.3.7'` and push the tag with `git push --tags`.
 After that, the github webinterface will allow you to make a release out of the new tag and include the binary file. 
 To make the zip file invoke `make projectname-version-binaries.zip` (Tab-autocomplete is your friend).

