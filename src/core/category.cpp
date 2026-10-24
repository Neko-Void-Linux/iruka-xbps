#include "category.h"

#include <cctype>
#include <initializer_list>

namespace {

std::string toLower(const std::string &s) {
    std::string out = s;
    for (char &c : out) c = static_cast<char>(std::tolower((unsigned char)c));
    return out;
}

bool containsAny(const std::string &hay, std::initializer_list<const char*> keys) {
    for (const char *k : keys) {
        if (hay.find(k) != std::string::npos) return true;
    }
    return false;
}

}

namespace category {

const std::vector<std::string> &all() {
    static const std::vector<std::string> cats = {
        "Multimedia",
        "Office",
        "Programming",
        "Education",
        "Graphics",
        "Network",
        "Games",
        "Security",
        "System",
        "Utilities",
        "Other",
    };
    return cats;
}

std::string classify(const std::string &name, const std::string &text) {
    const std::string n = toLower(name);
    const std::string t = toLower(text);


    if (containsAny(n, {"python", "perl", "ruby", "node", "rust", "golang",
                        "java", "jdk", "-devel", "-dev", "gcc", "clang", "cmake",
                        "meson", "ninja", "make", "git", "autoconf", "automake",
                        "libtool", "pkg-config", "gdb", "valgrind", "qmake"}) ||
        containsAny(t, {"compiler", "development library", "programming",
                        "debugger", "sdk", "build system"})) {
        return "Programming";
    }

    if (containsAny(n, {"ffmpeg", "gst", "vlc", "mpv", "mplayer", "audacious",
                        "rhythmbox", "spotify", "pulseaudio", "pipewire", "alsa",
                        "obs", "kdenlive", "handbrake", "sox", "lame", "flac",
                        "codec", "audio", "video", "music"}) ||
        containsAny(t, {"audio", "video", "music", "player", "multimedia",
                        "streaming", "codec"})) {
        return "Multimedia";
    }

    if (containsAny(n, {"gimp", "inkscape", "imagemagick", "graphviz", "blender",
                        "krita", "scribus", "feh", "imv", "raster", "svg"}) ||
        containsAny(t, {"image", "graphics", "photo", "drawing", "vector",
                        "3d model"})) {
        return "Graphics";
    }

    if (containsAny(n, {"libreoffice", "openoffice", "writer", "calc", "impress",
                        "abiword", "gnumeric", "pandoc", "texlive", "evince",
                        "zathura", "pdf", "office"}) ||
        containsAny(t, {"office", "word processor", "spreadsheet",
                        "presentation", "document", "pdf"})) {
        return "Office";
    }

    if (containsAny(t, {"education", "teaching", "learning", "mathematic",
                        "physics", "chemistry", "geography", "dictionary",
                        "science", "student", "school", "language learning"})) {
        return "Education";
    }

    if (containsAny(n, {"game", "dosbox", "steam", "lutris", "sdl"}) ||
        containsAny(t, {"game", "gaming", "arcade"})) {
        return "Games";
    }

    if (containsAny(n, {"firefox", "chromium", "chrome", "curl", "wget", "ssh",
                        "openssh", "rsync", "ftp", "transmission", "qbittorrent",
                        "deluge", "wireguard", "openvpn", "tailscale",
                        "networkmanager", "dns", "bind", "nginx", "apache",
                        "thunderbird", "irssi", "weechat", "telegram", "discord"}) ||
        containsAny(t, {"network", "browser", "web browser", "mail", "chat",
                        "torrent", "vpn", "ftp", "ssh", "dns", "web server"})) {
        return "Network";
    }

    if (containsAny(n, {"gpg", "gnupg", "openssl", "nmap", "iptables",
                        "nftables", "firejail", "keepass", "clamav"}) ||
        containsAny(t, {"security", "encryption", "cryptography", "firewall",
                        "password"})) {
        return "Security";
    }

    if (containsAny(n, {"kernel", "glibc", "systemd", "runit", "dinit", "grub",
                        "util-linux", "coreutils", "dbus", "udev", "firmware",
                        "driver", "xbps"}) ||
        containsAny(t, {"system", "kernel", "driver", "daemon", "boot", "init",
                        "core"})) {
        return "System";
    }

    if (containsAny(n, {"vim", "neovim", "emacs", "nano", "tmux", "screen",
                        "htop", "nload", "zip", "unzip", "tar", "gzip", "xz",
                        "zstd", "jq", "grep", "sed", "awk", "util"}) ||
        containsAny(t, {"utility", "tool", "editor", "terminal", "compression",
                        "archive", "shell", "file manager"})) {
        return "Utilities";
    }

    return "Other";
}

}
