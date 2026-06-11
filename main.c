#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Para usar strings
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

// Um pixel RGB (24 bits)
typedef struct
{
    unsigned char r, g, b;
} RGBPixel;

// Um pixel no formato RGBE (unsigned char)
typedef struct
{
  unsigned char r, g, b, e;
} RGBEPixel;

// Um pixel no formato RGBf (float)
typedef struct
{
  float r, g, b;
} RGBFPixel;

typedef struct
{
    int width, height;
    RGBPixel *pixels;
} ImgRGB;

// Uma imagem RGBF
typedef struct
{
    int width, height;
    RGBFPixel *pixels;
} ImgRGBF;

// Protótipos
void process(ImgRGBF* in, ImgRGB* out);
void carregaHeader(FILE* fp, ImgRGBF* img);
void carregaImagem(FILE* fp, ImgRGBF* img);

int main(int argc, char *argv[])
{
    // Inclua e processe os demais argumentos por linha de comando
    if (argc < 2)
    {
        printf("hdrvis [imagem .hdf]\n");
        exit(1);
    }

    // Imagens de entrada e saída
    ImgRGBF entrada;
    ImgRGB  saida;

    //
    // PASSO 1: Leitura da imagem
    // A leitura do header deve ser feita na função carregaHeader
    FILE* arq = fopen(argv[1], "rb"); // abre em formato binário
    carregaHeader(arq, &entrada);

    // Exibe as dimensões na tela, para conferência
    printf("Entrada   : %s %d x %d\n", argv[1], entrada.width, entrada.height);

    // A leitura do restante da imagem deve ser feita na função carregaImagem
    carregaImagem(arq, &entrada);

    // Cria imagem de saída e "zera" ela
    int tam = entrada.width * entrada.height;
    saida.pixels = malloc(tam * sizeof(RGBPixel));
    memset(saida.pixels, 0, tam * sizeof(RGBPixel));

    printf("Processando...\n");
    // Aplica todo o processamento necessário (exposição, etc) na função process
    // (acrescente mais parâmetros conforme a necessidade)
    process(&entrada, &saida);

    // Grava a imagem de saída como JPEG para conferência (usando a função stbi_write_jpg, com qualidade 90)
    stbi_write_jpg("saida.jpg", saida.width, saida.height, 3, saida.pixels, 90);

    free(entrada.pixels);
    free(saida.pixels);
}

// Executa todo o pipeline de processamento, grava saída em out->pixels
void process(ImgRGBF* in, ImgRGB* out)
{

}

// Esta função deverá ser utilizada para apenas ler o conteúdo do header
// e extrair a largura e altura da imagem
void carregaHeader(FILE* fp, ImgRGBF* img)
{

}

// Esta função deverá ser utilizada para carregar o restante
// da imagem (após ler o header e extrair a largura e altura corretamente)
// (não esqueça de alocar memória para os bytes no formato RGBE (unsigned char)
// e também para os bytes no formato RGBF (float) )
void carregaImagem(FILE* fp, ImgRGBF* img)
{

}

