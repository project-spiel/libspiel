FROM ubuntu:lunar

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
    python3-dbus \
    python3-tap \
    python3-gi \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    && apt-get clean -y \
    && apt-get autoremove -y \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME
