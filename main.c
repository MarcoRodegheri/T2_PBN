#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Para usar strings
#include <math.h>   // Para powf()
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

//
// Tipos de ponteiros de função usados no pipeline de processamento.
// Conforme pedido no enunciado, exposição, tone mapping (Reinhard/ACES)
// e correção gama são aplicados através de ponteiros de função.
//
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
        printf("  exposicao   : fator de exposicao em stops (ex: 0, 1.5, -2)\n");
        printf("  gama        : valor da correcao gama (ex: 2.2)\n");
        printf("  reinhard|aces : algoritmo de tone mapping desejado\n");
        exit(1);
    }

    // Lê o fator de exposição (em stops) e o valor de gama informados
    float exposicao = (float)atof(argv[2]);
    float gama = (float)atof(argv[3]);

    // Seleciona, via ponteiro de função, o algoritmo de tone mapping
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
//
// Ordem do pipeline (conforme o enunciado):
//   1. Aplicar o fator de exposição
//   2. Aplicar o algoritmo de tone mapping (Reinhard ou ACES)
//   3. Aplicar a correção gama
//   4. Converter o resultado para 24 bits
void process(ImgRGBF *in, ImgRGB *out, float exposicao, float gama, FuncToneMap toneMap)
{
    int tam = in->width * in->height;

    out->width = in->width;
    out->height = in->height;

    // Ponteiros de função para exposição e correção gama
    FuncExposicao funcExposicao = aplicaExposicao;
    FuncGama funcGama = aplicaGama;

    // PASSO 1: aplica o fator de exposição a cada componente de cada pixel
    for (int i = 0; i < tam; i++)
    {
        in->pixels[i].r = funcExposicao(in->pixels[i].r, exposicao);
        in->pixels[i].g = funcExposicao(in->pixels[i].g, exposicao);
        in->pixels[i].b = funcExposicao(in->pixels[i].b, exposicao);
    }

    // PASSO 2: tone mapping
    //
    // O algoritmo de Reinhard precisa do parâmetro Lwhite, calculado a
    // partir da luminância de todos os pixels da imagem (já após a
    // exposição). Lwhite é a maior luminância encontrada na imagem, ou
    // seja, o valor que será mapeado exatamente para o branco puro (1.0);
    // qualquer luminância igual ou superior a ela é levada para 1.0.
    // O algoritmo ACES não utiliza esse parâmetro (ele é simplesmente
    // ignorado pela função correspondente).
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

        // Evita divisão por zero caso a imagem seja totalmente preta
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

    // PASSO 3: aplica a correção gama
    for (int i = 0; i < tam; i++)
    {
        in->pixels[i].r = funcGama(in->pixels[i].r, gama);
        in->pixels[i].g = funcGama(in->pixels[i].g, gama);
        in->pixels[i].b = funcGama(in->pixels[i].b, gama);
    }

    // PASSO 4: converte o resultado (em [0,1]) para 24 bits (0..255)
    for (int i = 0; i < tam; i++)
    {
        out->pixels[i].r = converte8bits(in->pixels[i].r);
        out->pixels[i].g = converte8bits(in->pixels[i].g);
        out->pixels[i].b = converte8bits(in->pixels[i].b);
    }
}

// Esta função deverá ser utilizada para apenas ler o conteúdo do header
// e extrair a largura e altura da imagem
//
// Formato do header (arquivo .hdf):
//   - 3 bytes  : caracteres "HDF" (identificador do formato)
//   - 4 bytes  : largura da imagem (inteiro sem sinal)
//   - 4 bytes  : altura da imagem (inteiro sem sinal)
void carregaHeader(FILE *fp, ImgRGBF *img)
{
    char magic[4] = {0};

    // Lê os 3 caracteres "HDF" que identificam o formato
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

    // Lê largura e altura, ambos inteiros sem sinal de 4 bytes
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
//
// Cada pixel é armazenado em 4 bytes no formato RGBE: R, G, B (mantissas)
// e E (expoente). Para converter para float, calcula-se o fator
//   f = 2 ^ (E - 136)
// (136 = 128 + 8, conforme a codificação RGBE) e então:
//   R_float = R * f ,  G_float = G * f ,  B_float = B * f
void carregaImagem(FILE *fp, ImgRGBF *img)
{
    int tam = img->width * img->height;

    // Aloca memória para os pixels originais, no formato RGBE
    RGBEPixel *pixelsRGBE = malloc(tam * sizeof(RGBEPixel));

    // Lê todos os pixels (4 bytes cada) de uma só vez
    if (fread(pixelsRGBE, sizeof(RGBEPixel), tam, fp) != (size_t)tam)
    {
        printf("Erro: falha ao ler os pixels da imagem\n");
        exit(1);
    }

    // Aloca memória para a imagem convertida em ponto flutuante (RGBF)
    img->pixels = malloc(tam * sizeof(RGBFPixel));

    for (int i = 0; i < tam; i++)
    {
        // Fator de conversão calculado a partir do byte de mantissa (E)
        float f = powf(2.0f, (float)(pixelsRGBE[i].e - 136));

        img->pixels[i].r = pixelsRGBE[i].r * f;
        img->pixels[i].g = pixelsRGBE[i].g * f;
        img->pixels[i].b = pixelsRGBE[i].b * f;
    }

    free(pixelsRGBE);
}

// Aplica o fator de exposição (em "stops") a um componente de cor.
// Um stop corresponde a uma potência de 2: +1 stop dobra a exposição,
// -2 stops reduz a exposição para 1/4 do valor original.
float aplicaExposicao(float valor, float stops)
{
    return valor * powf(2.0f, stops);
}

// Calcula a luminância de um pixel usando os pesos de percepção
// humana padrão da norma ITU-R BT.709
float calculaLuminancia(RGBFPixel p)
{
    return 0.2126f * p.r + 0.7152f * p.g + 0.0722f * p.b;
}

// Algoritmo de tone mapping "Reinhard Global" (operador modificado,
// com ponto de branco / "white point").
//
// Referência: REINHARD, E. et al. "Photographic Tone Reproduction for
// Digital Images". ACM Transactions on Graphics, 2002. (operador
// estendido, que introduz o parâmetro Lwhite para permitir que valores
// de luminância sejam mapeados para o branco puro)
//
//   Ld = Lw * (1 + Lw / Lwhite^2) / (1 + Lw)
//
// Todo valor igual ou superior a Lwhite é levado diretamente para 1.0
float toneMapReinhard(float valor, float Lwhite)
{
    if (valor >= Lwhite)
    {
        return 1.0f;
    }

    return (valor * (1.0f + valor / (Lwhite * Lwhite))) / (1.0f + valor);
}

// Algoritmo de tone mapping ACES (Academy Color Encoding System).
//
// Primeiro reduz o valor do componente para 60% do original e, em
// seguida, aplica a curva ACES (aproximação de Narkowicz), garantindo
// que o resultado fique no intervalo [0, 1].
//
// O segundo parâmetro não é utilizado por este algoritmo (existe apenas
// para que ele possa ser chamado através do mesmo tipo de ponteiro de
// função usado pelo Reinhard).
float toneMapACES(float valor, float param)
{
    (void)param; // não utilizado pelo ACES

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

// Aplica a correção gama: out = in ^ (1 / gama)
float aplicaGama(float valor, float gama)
{
    if (valor < 0.0f)
    {
        valor = 0.0f;
    }

    return powf(valor, 1.0f / gama);
}

// Converte um componente de cor em ponto flutuante (na faixa [0,1])
// para um valor inteiro de 8 bits (0..255)
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