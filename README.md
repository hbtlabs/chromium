## What is it?

Fork of chromium/google-chrome to fix the white flash issue [explained in this video](http://youtu.be/HfJ_EwSTevI)


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

Package is for ubuntu64 precise and would install as chromium-beta 

[deb package](http://hbtlabs.com/chromium-browser-beta_57.0.2925.0-1_amd64.deb)
[libs to be unpacked](http://hbtlabs.com/chromium-libs.tar.bz2)

sudo dpkg -i package.deb
copy the libs manually and use ldconfig (view instructions below)


## How to disable black background?

chrome --disable-black-bg

Better alternative is to use a chrome extension that has access to the chrome:// pages and inject whatever stylesheet you want.
Example: [https://github.com/hbt/chromedotfiles](https://github.com/hbt/chromedotfiles)

Jonathan Timbaldi is working on turning all chrome pages dark [here](https://github.com/imatimba/darker-chromium)


## Windows Platform support

Windows binaries with this fix have been built. (Thanks to Jonathan)

[Instructions here](https://bugs.chromium.org/p/chromium/issues/detail?id=470669#c211)
[Binaries here (updated weekly)](https://www.dropbox.com/sh/7hjv18bo571kifm/AAD7wMz-cPLphfG1jBMKEzFIa?dl=0)
[MIRROR of chrome version 57](http://hbtlabs.com/windows-chrome-white-flash.zip)

## What about other platforms?

If there is demand, I will compile for other platforms. Since compiling for other platforms that I don't use requires installing mac/windows + building the project and its deps, it's not worth doing.
Feel free to do it and share the binaries. 


## How to build code base for windows?

[Instructions](https://github.com/henrypp/chromium/blob/master/building_chromium_gn.md)


## Why is the fix not working?

Try disabling your theme. Some themes e.g https://chrome.google.com/webstore/detail/developer-edition-dark/lglfmldlfmbbehalkgiglehhjblbfcjo are known to create a micro white flash

## Why is flash player not working?

[download flash player](https://get.adobe.com/flashplayer/?no_redirect) and load it manually

Example: 
`nohup /opt/chromium.org/chromium-beta/chrome  --new-window $1 --ppapi-flash-path=/home/hassen/Downloads/flash_player_ppapi_linux.x86_64/libpepflashplayer.so --allow-file-access %U  --disk-cache-dir="/tmp/ram" &> /dev/null &
`


## Why is netflix not working?

Chromium is not google chrome. It doesnt have the widevie plugin.
Check in chrome://plugins

## Why is the browser not syncing my bookmarks, extensions etc.?

Chromium doesnt come with google features. You can include them by create API keys on your google account.
[Instructions](https://www.chromium.org/developers/how-tos/api-keys)

## Where is the latest browser version and auto-updating?

The links enclosed point to chrome 57 binaries. The browser does not auto-update. 
You can compile the branch to the latest version or wait until binaries are released and linked on this README.

Main reasons this is not done frequently are:

- build instructions change 
- it takes time to compile + release
- this process is not automated


## Where are the macosx binaries?

None at the moment. Compile them and send them to me like Jonathan did.


## How to build and update codebase?

[chromium build instruction] (https://www.chromium.org/developers/how-tos/get-the-code)


```

# install depot tools

cd /media/hassen/linux-tmp/chromium
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:/media/hassen/linux-tmp/chromium/depot_tools

# include PATH to depot tools binaries in zsh config


# download chromium code base
fetch chromium

# update code base 
git pull origin master
sudo ./build/install-build-deps.sh
gclient runhooks
gclient fetch
gclient sync

# generate config 
gn args out/Release

is_component_build = true
is_debug = false
symbol_level = 0
enable_nacl = true
remove_webcore_debug_symbols = true
enable_linux_installer = true
#is_chrome_branded = true

gn gen out/Release

# build 
ninja -C out/Release chrome && ./out/Release/chrome

# launch chrome
./out/Release/chrome

#make changes in source code, compile and launch
ninja -C out/Release chrome && ./out/Release/chrome

```


## How to do a quick update + publish work?

```

# connect to remote and push 
sshlabs
cd /mnt/extra/chrome/src
git remote add gh_origin git@github.com:hbtlabs/chromium-white-flash-fix.git  


# update code and merge 
g co white-flash-fix
git fetch 
git pull origin master
git push gh_origin white-flash-fix

# local push after retrieving latest changes (needed since internet upload speed sucks)
git remote add gh_origin git@github.com:hbtlabs/chromium-white-flash-fix.git  
git push gh_origin white-flash-fix


```


## How to build deb files and install?

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

copies libs manually due to warning issues when producing deb



## What's next?

This fork will be flagged as deprecated the moment those bugs are solved. 

[list of improvements to be made](https://gist.github.com/hbt/94e527e6aba99baffba27259a98198b1#file-improvements-php)



