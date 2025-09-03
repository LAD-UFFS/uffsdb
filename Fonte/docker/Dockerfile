FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PS1='$(whoami):$(pwd)\\$'

# Instalar apenas o necessário para compilar e rodar
RUN apt-get update && apt-get install -y \
    build-essential \
    bison \
    flex \
    libreadline-dev \
    git \
    nano \
    && rm -rf /var/lib/apt/lists/*

# Opcional: sintaxe colorida no nano
RUN git clone https://github.com/scopatz/nanorc.git ~/.nano && \
    echo "include ~/.nano/*.nanorc" >> ~/.nanorc

# Definir diretório de trabalho
WORKDIR /app

# Copiar código-fonte para dentro da imagem
COPY .. .

# Compilar projeto
RUN make

# Executar o binário ao rodar o container
ENTRYPOINT ["bash"]
