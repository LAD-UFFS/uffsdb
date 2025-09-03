# Executando o projeto com Docker

Este projeto utiliza **Docker** e **Docker Compose** para facilitar a compilação e execução.

---

## Pré-requisitos
- [Docker](https://docs.docker.com/get-docker/) instalado
- [Docker Compose](https://docs.docker.com/compose/install/) instalado

---

## Passos para executar

1. Acesse a pasta `docker`:
   ```bash
   cd Fonte/docker
   ```

2. Faça o build e suba o container:
   ```bash
   docker compose up --build -d
   ```

3. Acesse o container via **bash**:
   ```bash
   docker compose exec uffsdb bash
   ```

4. Compile e execute o binário (caso ainda não esteja compilado):
   ```bash
   make
   ./uffsdb
   ```

---

## Observações
- O container já possui todas as dependências necessárias (`gcc`, `make`, `flex`, `bison`, `libreadline-dev` etc.).
- Você pode modificar o código em `Fonte/` e recompilar dentro do container com `make`.
- Para parar e remover os containers:
  ```bash
  docker compose down
  ```
