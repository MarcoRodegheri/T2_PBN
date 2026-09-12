<div align="center">

# Visualizador de Imagens HDR (Tone Mapping) — PUCRS

### Trabalho 2 da disciplina de Programação de Baixo Nível (PBN)

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
![Make](https://img.shields.io/badge/GNU_Make-A42E2B?style=for-the-badge&logo=gnu&logoColor=white)

</div>

---

## Sobre o projeto

Ferramenta de linha de comando em **C puro** desenvolvida como trabalho da disciplina de **Programação de Baixo Nível (PUCRS)**. O programa lê uma imagem em **HDR (High Dynamic Range)**, no formato binário customizado `.hdf`, e a converte para uma imagem comum de 8 bits por canal, aplicando o pipeline de processamento usado em fotografia HDR: **exposição**, **tone mapping** e **correção gama**.

Todo o processamento é feito manualmente sobre os pixels — leitura binária do header e dos dados HDR, decodificação do formato **RGBE**, conversão para ponto flutuante e reconversão para 8 bits — sem uso de bibliotecas de alto nível para o processamento (apenas [`stb_image_write`](https://github.com/nothings/stb) para gravar o JPEG final).

## Como funciona

1. **Leitura do arquivo `.hdf`** — o header é validado (`"HDF"`) e a largura/altura são lidas em binário.
2. **Decodificação RGBE → float** — cada pixel é armazenado no formato RGBE (3 bytes de cor + 1 byte de expoente compartilhado) e convertido para valores de radiância em ponto flutuante.
3. **Exposição** — os valores de radiância são multiplicados por `2^stops`, simulando o ajuste de exposição de uma câmera.
4. **Tone mapping** — os valores de HDR (que podem ultrapassar 1.0) são comprimidos para o intervalo exibível, usando um dos dois algoritmos:
   - **Reinhard** — normaliza com base na luminância máxima da cena (`Lwhite`).
   - **ACES** — curva filmica aproximada, usada como padrão na indústria de cinema/games.
5. **Correção gama** — aplica `valor^(1/gama)` para compensar a resposta não linear de monitores.
6. **Conversão para 8 bits** e gravação do resultado em `saida.jpg`.

## Uso

```bash
hdrvis [imagem.hdf] [exposicao] [gama] [reinhard|aces]
```

**Exemplo:**
```bash
./hdrvis imagens/memorial.hdf 1.5 2.2 aces
```

## Estrutura do projeto

```
T2_PBN/
├── main.c                     # Leitura do HDF, pipeline de tone mapping
├── include/
│   ├── stb_image.h             # Biblioteca de terceiros
│   └── stb_image_write.h       # Biblioteca de terceiros
├── imagens/                   # Imagens de teste (.hdf e referências .jpg)
├── Makefile                   # Build multiplataforma (gera o executável "hdrvis")
└── CMakeLists.txt             # Build alternativo via CMake
```

## Como rodar

**Usando Make:**
```bash
make
./hdrvis imagens/memorial.hdf 0 2.2 reinhard
```

**Usando CMake:**
```bash
mkdir build && cd build
cmake ..
make
./transmutator ../imagens/memorial.hdf 0 2.2 reinhard
```

O resultado é salvo como `saida.jpg`.

## Equipe

- Marco Antônio De Carli Rodegheri
- Roger Rozales Ehlert
