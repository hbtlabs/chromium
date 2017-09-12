## What is it?

Fork of chromium/google-chrome to fix the white flash issue [explained in this video](http://youtu.be/HfJ_EwSTevI)

Chromium version 57.0.2925.0


## What's the fix?

Changed the background color from white to black in chrome pages. Removes the flash.

There are 2 main bugs in white flash

1. when clicking links and opening them in new tabs or moving between tabs 
2. when opening new windows.

This fixes both of them in commit https://github.com/hbtlabs/chromium-white-flash-fix/commit/b1c4d8efd43fa46c61b9dc695f387f41a16947fb


## Why doesn't the chrome team merge this?

Because it replaces the white by black and most websites have a white background and most people don't use a dark theme. Hence, the white flash is a non issue for the majority of users.
Also, the webkit fix changes the background of chrome:// pages to black


## What is the chromium team doing about this issue?

Issue has been logged multiple times since https://bugs.chromium.org/p/chromium/issues/detail?id=1373
Most recent progress is https://bugs.chromium.org/p/chromium/issues/detail?id=470669
The issue has been broken down since they are attempting an algorithmic fix https://bugs.chromium.org/p/chromium/issues/detail?id=21798

Algorithmic fix: determining what's the background color of the current page or theme page then applying it during rendering instead of choosing white by default. 


## Where to download the binaries?

### Linux Ubuntu 14.04 x64

Package is for ubuntu64 precise and would install as chromium-beta 

[deb package](https://github.com/hbt/chromium-white-flash-docker/blob/master/data/chromium-browser-beta_57.0.2925.0-1_amd64.deb?raw=true)
[libs to be unpacked (optional)](https://github.com/hbt/chromium-white-flash-docker/blob/master/data/chromium-libs.tar.bz2?raw=true)


### Windows


Limited to version chrome v56

Jonathan Timbaldi is working on turning all chrome pages dark [here](https://github.com/imatimba/darker-chromium)
Windows binaries with this fix have been built.

[Instructions here](https://bugs.chromium.org/p/chromium/issues/detail?id=470669#c211)
[Binaries here](https://www.dropbox.com/sh/7hjv18bo571kifm/AAD7wMz-cPLphfG1jBMKEzFIa?dl=0)
[MIRROR of chrome version 56](http://hbtlabs.com/windows-chrome-white-flash.zip)

## What about other platforms?

If there is demand, I will compile for other platforms. Since compiling for other platforms that I don't use requires installing mac/windows + building the project and its deps, it's not worth doing.
Feel free to do it and share the binaries. 


## How to fix black background in chrome:// pages ?


Use a chrome extension that has access to the chrome:// pages and inject whatever stylesheet you want.
Example: [https://github.com/hbt/chromedotfiles](https://github.com/hbt/chromedotfiles)


## How to build code base for windows (from scratch)?

[Instructions](https://github.com/henrypp/chromium/blob/master/building_chromium_gn.md)


## Why is the fix not working?

Try disabling your theme. Some themes e.g https://chrome.google.com/webstore/detail/developer-edition-dark/lglfmldlfmbbehalkgiglehhjblbfcjo are known to create a micro white flash

## Why is flash player not working?

[download flash player](https://get.adobe.com/flashplayer/?no_redirect) and load it manually

Example: 

`
nohup /opt/chromium.org/chromium-beta/chrome  --disable-infobars --ppapi-flash-path=/home/hassen/programs/flash_player_ppapi_linux.x86_64/libpepflashplayer.so --allow-file-access %U  --disk-cache-dir="/tmp/ram"  &> /dev/null &
`


## Why is netflix not working?

Chromium is not google chrome. It doesnt have the widevine codec plugin.
Check in chrome://plugins

## Why is the browser not syncing my bookmarks, extensions etc.?

Chromium doesnt come with google features. You can include them by create API keys on your google account.
[Instructions](https://www.chromium.org/developers/how-tos/api-keys)

## Where is the latest browser version and auto-updating?

The links enclosed point to chrome 57 binaries. The browser does not auto-update. 
You can compile the branch to the latest version or wait until binaries are released and linked on this README.

Main reasons this is not done frequently are:

- code changes and this fix has to be adjusted (takes time)
- compile + release process is manual (takes time)



## Contributors Section

## How to build and update codebase?

[chromium build instruction] (https://www.chromium.org/developers/how-tos/get-the-code)

Build from Docker using [https://github.com/hbt/chromium-white-flash-docker] (https://github.com/hbt/chromium-white-flash-docker)


## How to do a quick update + publish work?

In latest versions, the code has changed and this fix might no longer apply. 

Steps:

- use docker image 
- select version and build
- fix any build issues
- apply white flash fix 
- produce deb files

Checklist:

- fix works (new tab, moving between tabs and new window)
- minimal side effects from fix (black background in chrome pages)
- deb files do not require libs  (view below)
- docker repo is up-to-date
- a branch with the right tags exists
- links in this readme are up-to-date


## How to build deb files and install?

For some chromium versions, building a deb file results in error (cannot link libraries)
Use these instructions to install the binary, copy the libraries and update the path

```
export IGNORE_DEPS_CHANGES=1
ninja -C out/Release  "chrome/installer/linux:beta_deb"
sudo dpkg -i chromium-browser-beta_57.0.2925.0-1_amd64.deb

# copy the libs manually
sudo -s
cd /opt/chromium.org/chromium-beta
mkdir libs
cp -R /media/hassen/linux-tmp/chromium/src/out/Release/*.so . 

# fix the path 
echo "/opt/chromium.org/chromium-beta/libs" > /etc/ld.so.conf.d/chrome_beta.conf

sudo ldconfig

# run browser
chromium-browser-beta

```



## Experiencing SSL issues? 

Getting "ERR_SSL_PROTOCOL_ERROR" or "This site can’t provide a secure connection" 
go to chrome://flags
Set TLS to 1.2


## What's next?

This fork will be flagged as deprecated the moment those bugs are solved. 
It's been a few years, so don't hold your breath.

