#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Para usar strings
#include <math.h>
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

typedef float (*FuncExposicao)(float valor, float stops);
typedef float (*FuncToneMap)(float valor, float param);
typedef float (*FuncGama)(float valor, float gama);

// Protótipos
void process(ImgRGBF *in, ImgRGB *out, float exposicao, float gama, FuncToneMap toneMap);
void carregaHeader(FILE *fp, ImgRGBF *img);
void carregaImagem(FILE *fp, ImgRGBF *img);

float aplicaExposicao(float valor, float stops);
float toneMapReinhard(float valor, float Lwhite);
float toneMapACES(float valor, float param);
float aplicaGama(float valor, float gama);
float calculaLuminancia(RGBFPixel p);
unsigned char converte8bits(float valor);

int main(int argc, char *argv[])
{
    // Inclua e processe os demais argumentos por linha de comando
    if (argc < 5)
    {
        printf("hdrvis [imagem .hdf] [exposicao] [gama] [reinhard|aces]\n");
        printf("  imagem .hdf : arquivo da imagem HDR no formato HDF\n");
        printf("  exposicao   : fator de exposicao (ex: 0, 1.5, -2)\n");
        printf("  gama        : valor da correcao gama (ex: 2.2)\n");
        printf("  reinhard|aces : algoritmo de tone mapping desejado\n");
        exit(1);
    }

    float exposicao = (float)atof(argv[2]);
    float gama = (float)atof(argv[3]);

    FuncToneMap toneMap;
    if (strcmp(argv[4], "reinhard") == 0)
    {
        toneMap = toneMapReinhard;
    }
    else if (strcmp(argv[4], "aces") == 0)
    {
        toneMap = toneMapACES;
    }
    else
    {
        printf("Algoritmo de tone mapping invalido: \"%s\" (use \"reinhard\" ou \"aces\")\n", argv[4]);
        exit(1);
    }

    // Imagens de entrada e saída
    ImgRGBF entrada;
    ImgRGB saida;

    //
    // PASSO 1: Leitura da imagem
    // A leitura do header deve ser feita na função carregaHeader
    FILE *arq = fopen(argv[1], "rb"); // abre em formato binário
    if (arq == NULL)
    {
        printf("Erro: nao foi possivel abrir o arquivo \"%s\"\n", argv[1]);
        exit(1);
    }

    carregaHeader(arq, &entrada);

    // Exibe as dimensões na tela, para conferência
    printf("Entrada   : %s %d x %d\n", argv[1], entrada.width, entrada.height);

    // A leitura do restante da imagem deve ser feita na função carregaImagem
    carregaImagem(arq, &entrada);
    fclose(arq);

    // Cria imagem de saída e "zera" ela
    int tam = entrada.width * entrada.height;
    saida.pixels = malloc(tam * sizeof(RGBPixel));
    memset(saida.pixels, 0, tam * sizeof(RGBPixel));

    printf("Processando...\n");
    printf("Exposicao : %.2f stop(s)\n", exposicao);
    printf("Gama      : %.2f\n", gama);
    printf("Tone map  : %s\n", argv[4]);

    // Aplica todo o processamento necessário (exposição, etc) na função process
    // (acrescente mais parâmetros conforme a necessidade)
    process(&entrada, &saida, exposicao, gama, toneMap);

    // Grava a imagem de saída como JPEG para conferência (usando a função stbi_write_jpg, com qualidade 90)
    stbi_write_jpg("saida.jpg", saida.width, saida.height, 3, saida.pixels, 90);
    printf("Imagem gravada em saida.jpg\n");

    free(entrada.pixels);
    free(saida.pixels);

    return 0;
}

// Executa todo o pipeline de processamento, grava saída em out->pixels
void process(ImgRGBF *in, ImgRGB *out, float exposicao, float gama, FuncToneMap toneMap)
{
    int tam = in->width * in->height;

    out->width = in->width;
    out->height = in->height;

    FuncExposicao funcExposicao = aplicaExposicao;
    FuncGama funcGama = aplicaGama;

    // Aplica exposicao
    for (int i = 0; i < tam; i++)
    {
        in->pixels[i].r = funcExposicao(in->pixels[i].r, exposicao);
        in->pixels[i].g = funcExposicao(in->pixels[i].g, exposicao);
        in->pixels[i].b = funcExposicao(in->pixels[i].b, exposicao);
    }

    // Tone mapping
    float Lwhite = 0.0f;

    if (toneMap == toneMapReinhard)
    {
        for (int i = 0; i < tam; i++)
        {
            float Y = calculaLuminancia(in->pixels[i]);
            if (Y > Lwhite)
            {
                Lwhite = Y;
            }
        }

        // Evita div/0
        if (Lwhite <= 0.0f)
        {
            Lwhite = 1.0f;
        }

        printf("Lwhite (Reinhard) = %f\n", Lwhite);
    }

    for (int i = 0; i < tam; i++)
    {
        in->pixels[i].r = toneMap(in->pixels[i].r, Lwhite);
        in->pixels[i].g = toneMap(in->pixels[i].g, Lwhite);
        in->pixels[i].b = toneMap(in->pixels[i].b, Lwhite);
    }

    // Aplica gama
    for (int i = 0; i < tam; i++)
    {
        in->pixels[i].r = funcGama(in->pixels[i].r, gama);
        in->pixels[i].g = funcGama(in->pixels[i].g, gama);
        in->pixels[i].b = funcGama(in->pixels[i].b, gama);
    }

    // Converte pra 24 bits
    for (int i = 0; i < tam; i++)
    {
        out->pixels[i].r = converte8bits(in->pixels[i].r);
        out->pixels[i].g = converte8bits(in->pixels[i].g);
        out->pixels[i].b = converte8bits(in->pixels[i].b);
    }
}

// Esta função deverá ser utilizada para apenas ler o conteúdo do header
// e extrair a largura e altura da imagem
void carregaHeader(FILE *fp, ImgRGBF *img)
{
    char magic[4] = {0};

    if (fread(magic, sizeof(char), 3, fp) != 3)
    {
        printf("Erro: falha ao ler o cabecalho do arquivo\n");
        exit(1);
    }

    if (strcmp(magic, "HDF") != 0)
    {
        printf("Erro: arquivo nao esta no formato HDF esperado (magic = \"%s\")\n", magic);
        exit(1);
    }

    unsigned int largura, altura;

    if (fread(&largura, sizeof(unsigned int), 1, fp) != 1 ||
        fread(&altura, sizeof(unsigned int), 1, fp) != 1)
    {
        printf("Erro: falha ao ler as dimensoes da imagem\n");
        exit(1);
    }

    img->width = (int)largura;
    img->height = (int)altura;
}

// Esta função deverá ser utilizada para carregar o restante
// da imagem (após ler o header e extrair a largura e altura corretamente)
// (não esqueça de alocar memória para os bytes no formato RGBE (unsigned char)
// e também para os bytes no formato RGBF (float) )
void carregaImagem(FILE *fp, ImgRGBF *img)
{
    int tam = img->width * img->height;

    RGBEPixel *pixelsRGBE = malloc(tam * sizeof(RGBEPixel));

    if (fread(pixelsRGBE, sizeof(RGBEPixel), tam, fp) != (size_t)tam)
    {
        printf("Erro: falha ao ler os pixels da imagem\n");
        exit(1);
    }

    img->pixels = malloc(tam * sizeof(RGBFPixel));

    for (int i = 0; i < tam; i++)
    {
        float f = powf(2.0f, (float)(pixelsRGBE[i].e - 136));

        img->pixels[i].r = pixelsRGBE[i].r * f;
        img->pixels[i].g = pixelsRGBE[i].g * f;
        img->pixels[i].b = pixelsRGBE[i].b * f;
    }

    free(pixelsRGBE);
}

// Exposicao
float aplicaExposicao(float valor, float stops)
{
    return valor * powf(2.0f, stops);
}

// Luminancia
float calculaLuminancia(RGBFPixel p)
{
    return 0.2126f * p.r + 0.7152f * p.g + 0.0722f * p.b;
}

// Reinhard
float toneMapReinhard(float valor, float Lwhite)
{
    if (valor >= Lwhite)
    {
        return 1.0f;
    }

    return (valor * (1.0f + valor / (Lwhite * Lwhite))) / (1.0f + valor);
}

// ACES
float toneMapACES(float valor, float param)
{
    (void)param;

    valor = valor * 0.6f;

    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    float resultado = (valor * (a * valor + b)) / (valor * (c * valor + d) + e);

    if (resultado < 0.0f)
    {
        resultado = 0.0f;
    }
    if (resultado > 1.0f)
    {
        resultado = 1.0f;
    }

    return resultado;
}

// Correcao Gama
float aplicaGama(float valor, float gama)
{
    if (valor < 0.0f)
    {
        valor = 0.0f;
    }

    return powf(valor, 1.0f / gama);
}

// Float para 8 bits
unsigned char converte8bits(float valor)
{
    if (valor < 0.0f)
    {
        valor = 0.0f;
    }
    if (valor > 1.0f)
    {
        valor = 1.0f;
    }

    return (unsigned char)(valor * 255);
}