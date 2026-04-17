FROM espressif/idf:v5.3.5

ARG DEBIAN_FRONTEND=noninteractive
ARG NODE_VERSION=22.14.0
ARG CODEX_VERSION=0.121.0
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000
ARG TARGETARCH

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ENV IDF_PATH=/opt/esp/idf
ENV HOME=/home/vscode

# Keep the image small: add only the tools this repo needs for day-to-day work.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    ca-certificates \
    cppcheck \
    curl \
    git \
    openssh-client \
    ripgrep \
    unzip \
    wget \
    xz-utils \
    zsh \
    && rm -rf /var/lib/apt/lists/*

# Install a recent Node.js runtime so the Codex CLI can run in the container.
RUN case "${TARGETARCH}" in \
    amd64) node_arch="x64" ;; \
    arm64) node_arch="arm64" ;; \
    *) echo "Unsupported architecture: ${TARGETARCH}" >&2; exit 1 ;; \
    esac \
    && curl -fsSLo /tmp/node.tar.xz "https://nodejs.org/dist/v${NODE_VERSION}/node-v${NODE_VERSION}-linux-${node_arch}.tar.xz" \
    && tar -xJf /tmp/node.tar.xz -C /usr/local --strip-components=1 \
    && rm -f /tmp/node.tar.xz

RUN npm install -g "@openai/codex@${CODEX_VERSION}"

# Install uv for the repo's Python workflow.
RUN curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR=/usr/local/bin sh

RUN groupadd --gid "${USER_GID}" "${USERNAME}" \
    && useradd --uid "${USER_UID}" --gid "${USER_GID}" --home-dir "/home/${USERNAME}" --no-create-home -s /bin/bash "${USERNAME}" \
    && usermod -aG dialout "${USERNAME}" \
    && groupadd -f plugdev \
    && usermod -aG plugdev "${USERNAME}" \
    && chown -R "${USER_UID}:${USER_GID}" "/home/${USERNAME}"

COPY .devcontainer/source_env.sh /etc/profile.d/source_env.sh
RUN chmod 0755 /etc/profile.d/source_env.sh && \
    echo '. /etc/profile.d/source_env.sh' >> /etc/bash.bashrc && \
    echo '. /etc/profile.d/source_env.sh' > /etc/zsh/zshrc

WORKDIR /workspaces/embedded_ins_daq
