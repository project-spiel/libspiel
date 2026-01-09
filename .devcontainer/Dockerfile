FROM ubuntu:plucky

ARG DEBIAN_FRONTEND=noninteractive
ARG USERNAME=ubuntu

RUN apt-get update \
    && apt-get install -y \
    git \
    sudo \
    meson \
    gi-docgen \
    libgirepository1.0-dev \
    dbus \
    libdbus-glib-1-dev \
    python3-dasbus \
    python3-tap \
    python3-gi \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-good \
    && apt-get clean -y \
    && apt-get autoremove -y \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME
