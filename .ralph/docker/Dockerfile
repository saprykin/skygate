# syntax=docker/dockerfile:1

ARG BASE_PLATFORM=linux/arm64
FROM --platform=${BASE_PLATFORM} ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG NODE_MAJOR=22
ARG QT_VERSION=6.11.0
ARG QT_HOST=linux_arm64
ARG QT_ARCH=linux_gcc_arm64
ARG QT_INSTALL_ARCH=gcc_arm64
ARG USERNAME=dev
ARG USER_UID=1001
ARG USER_GID=${USER_UID}

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        ccache \
        clang-format \
        curl \
        dbus \
        dnsutils \
        g++ \
        gcc \
        gdb \
        git \
        git-lfs \
        gnupg \
        iproute2 \
        iputils-ping \
        jq \
        libegl1 \
        libfontconfig1 \
        libgl1 \
        libgl1-mesa-dev \
        libglib2.0-0 \
        libgtk-3-0 \
        libpulse0 \
        libvulkan1 \
        libx11-6 \
        libxcb-cursor0 \
        libxcb-icccm4 \
        libxcb-image0 \
        libxcb-keysyms1 \
        libxcb-randr0 \
        libxcb-render-util0 \
        libxcb-shape0 \
        libxcb-xinerama0 \
        libxcb-xkb1 \
        libxkbcommon-x11-0 \
        make \
        netcat-openbsd \
        ninja-build \
        openssh-client \
        pkg-config \
        python3 \
        python3-pip \
        python3-venv \
        ripgrep \
        sudo \
        unzip \
        vim \
        wget \
        xz-utils \
        zlib1g-dev \
    && install -d -m 0755 /etc/apt/keyrings \
    && curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key \
        | gpg --dearmor -o /etc/apt/keyrings/nodesource.gpg \
    && echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg arch=$(dpkg --print-architecture)] https://deb.nodesource.com/node_${NODE_MAJOR}.x nodistro main" \
        > /etc/apt/sources.list.d/nodesource.list \
    && curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
        | gpg --dearmor -o /etc/apt/keyrings/githubcli-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
        > /etc/apt/sources.list.d/github-cli.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        cmake \
        gh \
        nodejs \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/aqt \
    && /opt/aqt/bin/pip install --no-cache-dir --upgrade pip aqtinstall \
    && ln -s /opt/aqt/bin/aqt /usr/local/bin/aqt

RUN aqt install-qt "${QT_HOST}" desktop "${QT_VERSION}" "${QT_ARCH}" \
        -O /opt/Qt \
        -m qtpositioning \
    && /opt/Qt/"${QT_VERSION}"/"${QT_INSTALL_ARCH}"/bin/qt-cmake --version

RUN npm install -g @openai/codex @anthropic-ai/claude-code \
    && codex --version \
    && claude --version \
    && gh --version \
    && cmake --version \
    && gcc --version \
    && g++ --version

RUN if getent group "${USER_GID}" > /dev/null; then \
        groupName="$(getent group "${USER_GID}" | cut -d: -f1)"; \
    else \
        groupadd --gid "${USER_GID}" "${USERNAME}"; \
        groupName="${USERNAME}"; \
    fi \
    && if getent passwd "${USER_UID}" > /dev/null; then \
        existingUser="$(getent passwd "${USER_UID}" | cut -d: -f1)"; \
        usermod --login "${USERNAME}" --home /home/"${USERNAME}" --move-home "${existingUser}"; \
    else \
        useradd --uid "${USER_UID}" --gid "${groupName}" -m "${USERNAME}"; \
    fi \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/"${USERNAME}" \
    && chmod 0440 /etc/sudoers.d/"${USERNAME}"

ENV QT_ROOT=/opt/Qt/${QT_VERSION}/${QT_INSTALL_ARCH}
ENV CMAKE_PREFIX_PATH=${QT_ROOT}
ENV PATH=${QT_ROOT}/bin:/home/${USERNAME}/.npm-global/bin:${PATH}

WORKDIR /workspace
USER ${USERNAME}

CMD ["/bin/bash"]
