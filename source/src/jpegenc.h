#ifndef JPEGENC
#define JPEGENC

// Toca: JPEG encoder, jpeglib independent  ***EXPERIMENTAL***
// Based on work by Cristian Cuturicu
// http://www.wotsit.org/download.asp?f=jpeg&sc=335837440

typedef unsigned char BYTE;
typedef signed char SBYTE;
typedef signed short int SWORD;
typedef unsigned short int WORD;
typedef unsigned long int DWORD;
typedef signed long int SDWORD;

typedef struct { BYTE B,G,R; } colorRGB;
typedef struct { BYTE length; WORD value;} bitstring;

#define  Y(R,G,B) ((BYTE)( (YRtab[(R)]+YGtab[(G)]+YBtab[(B)])>>16 ) - 128);
#define Cb(R,G,B) ((BYTE)( (CbRtab[(R)]+CbGtab[(G)]+CbBtab[(B)])>>16 ));
#define Cr(R,G,B) ((BYTE)( (CrRtab[(R)]+CrGtab[(G)]+CrBtab[(B)])>>16 ));

#define writebyte(b) fputc((b),fp_jpeg_stream);
#define writeword(w) writebyte((w)/256);writebyte((w)%256);

struct APP0infotype
{   
    WORD marker;
    WORD length;
    BYTE JFIFsignature[5];
    BYTE versionhi;
    BYTE versionlo;
    BYTE xyunits;
    WORD xdensity;
    WORD ydensity;
    BYTE thumbnwidth;
    BYTE thumbnheight;
    APP0infotype()
    {
        marker = 0xFFE0;
        length = 16;
        JFIFsignature[0] ='J';
        JFIFsignature[1] ='F';
        JFIFsignature[2] ='I';
        JFIFsignature[3] ='F';
        JFIFsignature[4] = 0;
        versionhi = 1;
        versionlo = 1;
        xyunits = 0;
        xdensity = 1;
        ydensity = 1;
        thumbnwidth = 0;
        thumbnheight = 0;
    };
};

struct  SOF0infotype
{
    WORD marker;
    WORD length;
    BYTE precision ;
    WORD height ;
    WORD width;
    BYTE nrofcomponents;
    BYTE IdY;
    BYTE HVY;
    BYTE QTY;
    BYTE IdCb;
    BYTE HVCb;
    BYTE QTCb;
    BYTE IdCr;
    BYTE HVCr;
    BYTE QTCr;
    
    SOF0infotype()
    {
         marker = 0xFFC0;
         length = 17;
         precision = 8;
         height = 0;
         width = 0;
         nrofcomponents = 3;
         IdY = 1;
         HVY = 0x11;
         QTY = 0;
         IdCb = 2;
         HVCb = 0x11;
         QTCb = 1;
         IdCr = 3;
         HVCr = 0x11;
         QTCr = 1;
    }
};

struct DQTinfotype
{
    WORD marker;
    WORD length;
    BYTE QTYinfo;
    BYTE Ytable[64];
    BYTE QTCbinfo;
    BYTE Cbtable[64];        
    DQTinfotype()
    {
        marker = 0xFFDB;
        length = 132;
        QTYinfo = 0;
        Ytable[0] = 0;
        QTCbinfo = 1;
        Cbtable[0] = 0;
    }
};

struct DHTinfotype
{
    WORD marker;
    WORD length;
    BYTE HTYDCinfo;
    BYTE YDC_nrcodes[16];
    BYTE YDC_values[12];
    BYTE HTYACinfo;
    BYTE YAC_nrcodes[16];
    BYTE YAC_values[162];
    BYTE HTCbDCinfo;
    BYTE CbDC_nrcodes[16];
    BYTE CbDC_values[12];
    BYTE HTCbACinfo;
    BYTE CbAC_nrcodes[16];
    BYTE CbAC_values[162];            
    DHTinfotype()
    {
        marker = 0xFFC4;
        length = 0x01A2;
        HTYDCinfo = 0;
        YDC_nrcodes[0] = 0;
        YDC_values[0] = 0;
        HTYACinfo = 0x10;
        YAC_nrcodes[0] = 0;
        YAC_values[0] = 0;
        HTCbDCinfo = 1;
        CbDC_nrcodes[0] = 0;
        CbDC_values[0] = 0;
        HTCbACinfo = 0x11;
        CbAC_nrcodes[0] = 0;
        CbAC_values[0] = 0;
    }
};

struct SOSinfotype
{
    WORD marker;
    WORD length;
    BYTE nrofcomponents;
    BYTE IdY;
    BYTE HTY;
    BYTE IdCb;
    BYTE HTCb;
    BYTE IdCr;
    BYTE HTCr;
    BYTE Ss,Se,Bf;            
    SOSinfotype()
    {
        marker = 0xFFDA;
        length = 12;
        nrofcomponents = 3;
        IdY = 1;
        HTY = 0;
        IdCb = 2;
        HTCb = 0x11;
        IdCr = 3;
        HTCr = 0x11;
        Ss = 0;
        Se = 0x3F;
        Bf = 0;
    }
};
       
class jpegenc
{
    private:
        static BYTE zigzag[];
        static BYTE std_luminance_qt[];
        static BYTE std_chrominance_qt[];
        static BYTE std_dc_luminance_nrcodes[];
        static BYTE std_dc_luminance_values[];
        static BYTE std_dc_chrominance_nrcodes[];
        static BYTE std_dc_chrominance_values[];
        static BYTE std_ac_luminance_nrcodes[];
        static BYTE std_ac_luminance_values[];
        static BYTE std_ac_chrominance_nrcodes[];
        static BYTE std_ac_chrominance_values[];
        static WORD mask[];
        
        APP0infotype APP0info;
        SOF0infotype SOF0info;
        DQTinfotype DQTinfo;
        SOSinfotype SOSinfo;
        DHTinfotype DHTinfo;
       
        BYTE bytenew;
        SBYTE bytepos;

        void write_APP0info(void);
        void write_SOF0info(void);
        void write_DQTinfo(void);
        void set_quant_table(BYTE *basic_table, BYTE scale_factor, BYTE *newtable);
        void set_DQTinfo(int quality);
        void write_DHTinfo(void);
        void set_DHTinfo(void);
        void write_SOSinfo(void);
        void write_comment(BYTE *comment);
        void writebits(bitstring bs);
        void compute_Huffman_table(BYTE *nrcodes, BYTE *std_table, bitstring *HT);
        void init_Huffman_tables(void);
        void exitmessage(const char *error_message);
        void set_numbers_category_and_bitcode();
        void precalculate_YCbCr_tables();
        void prepare_quant_tables();
        void fdct_and_quantization(SBYTE *data, float *fdtbl, SWORD *outdata);
        
        void process_DU(SBYTE *ComponentDU, float *fdtbl,SWORD *DC, bitstring *HTDC, bitstring *HTAC);        
        void load_data_units_from_RGB_buffer(WORD xpos, WORD ypos);
        
        void main_encoder(void);
        void init_all(int qfactor);
        
    public:
        jpegenc()
        {
            bytenew = 0;
            bytepos = 7;
        }

        bitstring  YDC_HT[12];
        bitstring CbDC_HT[12];
        bitstring  YAC_HT[256];
        bitstring CbAC_HT[256];

        BYTE *category_alloc;
        BYTE *category;
        bitstring *bitcode_alloc;
        bitstring *bitcode;

        SDWORD  YRtab[256],  YGtab[256],  YBtab[256];
        SDWORD CbRtab[256], CbGtab[256], CbBtab[256];
        SDWORD CrRtab[256], CrGtab[256], CrBtab[256];
        float  fdtbl_Y[64];
        float fdtbl_Cb[64];

        SBYTE  YDU[64];
        SBYTE CbDU[64];
        SBYTE CrDU[64];
        SWORD DU_DCT[64];
        SWORD DU[64];

        colorRGB *RGB_buffer;   //image to be encoded
        WORD width, height;     //image dimensions divisible by 8

        FILE *fp_jpeg_stream;
        
        int encode(const char *filename, colorRGB *pixels, int iw, int ih, int jpegquality);
};

BYTE jpegenc::zigzag[64] = {
        0, 1, 5, 6,14,15,27,28,
        2, 4, 7,13,16,26,29,42,
        3, 8,12,17,25,30,41,43,
        9,11,18,24,31,40,44,53,
        10,19,23,32,39,45,52,54,
        20,22,33,38,46,51,55,60,
        21,34,37,47,50,56,59,61,
        35,36,48,49,57,58,62,63
};

BYTE jpegenc::std_luminance_qt[64] = {
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99
};

BYTE jpegenc::std_chrominance_qt[64] = {
    17,  18,  24,  47,  99,  99,  99,  99,
    18,  21,  26,  66,  99,  99,  99,  99,
    24,  26,  56,  99,  99,  99,  99,  99,
    47,  66,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
};

BYTE jpegenc::std_dc_luminance_nrcodes[17] = {0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
BYTE jpegenc::std_dc_luminance_values[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
BYTE jpegenc::std_dc_chrominance_nrcodes[17] = {0,0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
BYTE jpegenc::std_dc_chrominance_values[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
BYTE jpegenc::std_ac_luminance_nrcodes[17] = {0,0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d };
BYTE jpegenc::std_ac_luminance_values[162] = {
      0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
      0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
      0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
      0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
      0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
      0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
      0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
      0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
      0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
      0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
      0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
      0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
      0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
      0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
      0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
      0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
      0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
      0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
      0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa
};

BYTE jpegenc::std_ac_chrominance_nrcodes[17] = {0,0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77};
BYTE jpegenc::std_ac_chrominance_values[162] = {
      0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
      0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
      0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
      0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
      0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
      0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
      0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
      0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
      0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
      0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
      0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
      0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
      0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
      0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
      0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
      0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
      0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
      0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
      0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
      0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa
};

WORD jpegenc::mask[16] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};


//******************************************************************************
//******************************************************************************
// Toca: TODO: the following goes into jpegenc.cpp or some other .cpp file

void jpegenc::write_APP0info() //Nothing to overwrite for APP0info
{
    writeword(APP0info.marker);
    writeword(APP0info.length);
    writebyte('J');
    writebyte('F');
    writebyte('I');
    writebyte('F');
    writebyte(0);
    writebyte(APP0info.versionhi);
    writebyte(APP0info.versionlo);
    writebyte(APP0info.xyunits);
    writeword(APP0info.xdensity);
    writeword(APP0info.ydensity);
    writebyte(APP0info.thumbnwidth);
    writebyte(APP0info.thumbnheight);
}

void jpegenc::write_SOF0info()
{
    writeword(SOF0info.marker);
    writeword(SOF0info.length);
    writebyte(SOF0info.precision);
    writeword(SOF0info.height); //Image height
    writeword(SOF0info.width);  //Image width
    writebyte(SOF0info.nrofcomponents);
    writebyte(SOF0info.IdY);
    writebyte(SOF0info.HVY);
    writebyte(SOF0info.QTY);
    writebyte(SOF0info.IdCb);
    writebyte(SOF0info.HVCb);
    writebyte(SOF0info.QTCb);
    writebyte(SOF0info.IdCr);
    writebyte(SOF0info.HVCr);
    writebyte(SOF0info.QTCr);
}

void jpegenc::write_DQTinfo()
{
    BYTE i;
    writeword(DQTinfo.marker);
    writeword(DQTinfo.length);
    writebyte(DQTinfo.QTYinfo);
    for (i=0; i<64; i++)
        writebyte(DQTinfo.Ytable[i]);
    writebyte(DQTinfo.QTCbinfo);
    for (i=0; i<64; i++)
        writebyte(DQTinfo.Cbtable[i]);
}

void jpegenc::set_quant_table(BYTE *basic_table, BYTE scale_factor, BYTE *newtable)
{
    BYTE i;
    long temp;
    for (i=0; i<64; i++) 
    {
        temp = ((long) basic_table[i] * scale_factor + 50L) / 100L;
        if (temp <= 0L) temp = 1L;
        if (temp > 255L) temp = 255L; 
        newtable[zigzag[i]] = (BYTE) temp;
    }
}

void jpegenc::set_DQTinfo(int quality)
{
    // Convert user 0-100 rating to percentage scaling
    if (quality <= 0) quality = 1;
    if (quality > 100) quality = 100;
    if (quality < 50) quality = 5000 / quality;
    else quality = 200 - quality*2;
    DQTinfo.marker = 0xFFDB;
    DQTinfo.length = 132;
    DQTinfo.QTYinfo = 0;
    DQTinfo.QTCbinfo = 1;
    set_quant_table(std_luminance_qt, quality, DQTinfo.Ytable);
    set_quant_table(std_chrominance_qt, quality, DQTinfo.Cbtable);
}

void jpegenc::write_DHTinfo()
{
    BYTE i;
    writeword(DHTinfo.marker);
    writeword(DHTinfo.length);
    writebyte(DHTinfo.HTYDCinfo);
    for (i=0; i<16; i++)
        writebyte(DHTinfo.YDC_nrcodes[i]);
    for (i=0; i<12; i++)
        writebyte(DHTinfo.YDC_values[i]);
    writebyte(DHTinfo.HTYACinfo);
    for (i=0; i<16; i++)
        writebyte(DHTinfo.YAC_nrcodes[i]);
    for (i=0; i<162; i++)
        writebyte(DHTinfo.YAC_values[i]);
    writebyte(DHTinfo.HTCbDCinfo);
    for (i=0; i<16; i++)
        writebyte(DHTinfo.CbDC_nrcodes[i]);
    for (i=0; i<12; i++)
        writebyte(DHTinfo.CbDC_values[i]);
    writebyte(DHTinfo.HTCbACinfo);
    for (i=0; i<16; i++)
        writebyte(DHTinfo.CbAC_nrcodes[i]);
    for (i=0; i<162; i++)
        writebyte(DHTinfo.CbAC_values[i]);
}

void jpegenc::set_DHTinfo()
{
    BYTE i;
    DHTinfo.marker = 0xFFC4;
    DHTinfo.length = 0x01A2;
    DHTinfo.HTYDCinfo = 0;
    for (i=0; i<16; i++)
        DHTinfo.YDC_nrcodes[i] = std_dc_luminance_nrcodes[i+1];
    for (i=0; i<12; i++)
        DHTinfo.YDC_values[i] = std_dc_luminance_values[i];
    DHTinfo.HTYACinfo = 0x10;
    for (i=0; i<16; i++)
        DHTinfo.YAC_nrcodes[i] = std_ac_luminance_nrcodes[i+1];
    for (i=0; i<162; i++)
        DHTinfo.YAC_values[i] = std_ac_luminance_values[i];
    DHTinfo.HTCbDCinfo = 1;
    for (i=0; i<16; i++)
        DHTinfo.CbDC_nrcodes[i] = std_dc_chrominance_nrcodes[i+1];
    for (i=0; i<12; i++)
        DHTinfo.CbDC_values[i] = std_dc_chrominance_values[i];
    DHTinfo.HTCbACinfo = 0x11;
    for (i=0; i<16; i++)
        DHTinfo.CbAC_nrcodes[i] = std_ac_chrominance_nrcodes[i+1];
    for (i=0; i<162; i++)
        DHTinfo.CbAC_values[i] = std_ac_chrominance_values[i];
}

void jpegenc::write_SOSinfo() //Nothing to overwrite for SOSinfo
{
    writeword(SOSinfo.marker);
    writeword(SOSinfo.length);
    writebyte(SOSinfo.nrofcomponents);
    writebyte(SOSinfo.IdY);
    writebyte(SOSinfo.HTY);
    writebyte(SOSinfo.IdCb);
    writebyte(SOSinfo.HTCb);
    writebyte(SOSinfo.IdCr);
    writebyte(SOSinfo.HTCr);
    writebyte(SOSinfo.Ss);
    writebyte(SOSinfo.Se);
    writebyte(SOSinfo.Bf);
}

void jpegenc::write_comment(BYTE *comment)
{
    WORD i, length;
    writeword(0xFFFE);
    length = strlen((const char *)comment);
    writeword(length + 2);
    for (i=0; i<length; i++) 
        writebyte(comment[i]);
}

void jpegenc::writebits(bitstring bs) // A portable version; it should be done in assembler
{
    WORD value;
    SBYTE posval;
    value = bs.value;
    posval = bs.length - 1;
    while (posval >= 0)
    {
        if (value & mask[posval]) 
            bytenew |= mask[bytepos];
        posval--;
        bytepos--;
        if (bytepos < 0) 
        { 
            if (bytenew == 0xFF) 
            {
                writebyte(0xFF);
                writebyte(0);
            }
            else writebyte(bytenew);

            bytepos = 7;
            bytenew = 0;
        }
    }
}

void jpegenc::compute_Huffman_table(BYTE *nrcodes, BYTE *std_table, bitstring *HT)
{
    BYTE k,j;
    BYTE pos_in_table;
    WORD codevalue;
    codevalue = 0; 
    pos_in_table = 0;
    for (k=1; k<=16; k++)
    {
        for (j=1; j<=nrcodes[k]; j++) 
        {
            HT[std_table[pos_in_table]].value = codevalue;
            HT[std_table[pos_in_table]].length = k;
            pos_in_table++;
            codevalue++;
        }
        codevalue <<= 1;
    }
}

void jpegenc::init_Huffman_tables()
{
    compute_Huffman_table(std_dc_luminance_nrcodes, std_dc_luminance_values, YDC_HT);
    compute_Huffman_table(std_ac_luminance_nrcodes, std_ac_luminance_values, YAC_HT);
    compute_Huffman_table(std_dc_chrominance_nrcodes, std_dc_chrominance_values, CbDC_HT);
    compute_Huffman_table(std_ac_chrominance_nrcodes, std_ac_chrominance_values, CbAC_HT);
}

void jpegenc::exitmessage(const char *error_message)
{
    printf("%s\n",error_message);
    exit(EXIT_FAILURE);
}

void jpegenc::set_numbers_category_and_bitcode()
{
    SDWORD nr;
    SDWORD nrlower, nrupper;
    BYTE cat;
    category_alloc = (BYTE *)malloc(65535*sizeof(BYTE));
    if (category_alloc == NULL) exitmessage("Not enough memory.");
    category = category_alloc + 32767; 
    bitcode_alloc=(bitstring *)malloc(65535*sizeof(bitstring));
    if (bitcode_alloc==NULL) exitmessage("Not enough memory.");
    bitcode = bitcode_alloc + 32767;
    nrlower = 1;
    nrupper = 2;
    for (cat=1; cat<=15; cat++) 
    {
        for (nr=nrlower; nr<nrupper; nr++)
        { 
            category[nr] = cat;
            bitcode[nr].length = cat;
            bitcode[nr].value = (WORD)nr;
        }
        for (nr=-(nrupper-1); nr<=-nrlower; nr++)
        { 
            category[nr] = cat;
            bitcode[nr].length = cat;
            bitcode[nr].value = (WORD)(nrupper-1+nr);
        }
        nrlower <<= 1;
        nrupper <<= 1;
    }
}

void jpegenc::precalculate_YCbCr_tables()
{
    WORD R,G,B;
    for (R=0; R<256; R++) 
    {
        YRtab[R] = (SDWORD)(65536*0.299+0.5)*R;
        CbRtab[R] = (SDWORD)(65536*-0.16874+0.5)*R;
        CrRtab[R] = (SDWORD)(32768)*R;
    }
    for (G=0; G<256; G++) 
    {
        YGtab[G] = (SDWORD)(65536*0.587+0.5)*G;
        CbGtab[G] = (SDWORD)(65536*-0.33126+0.5)*G;
        CrGtab[G] = (SDWORD)(65536*-0.41869+0.5)*G;
    }
    for (B=0; B<256; B++) 
    {
        YBtab[B] = (SDWORD)(65536*0.114+0.5)*B;
        CbBtab[B] = (SDWORD)(32768)*B;
        CrBtab[B] = (SDWORD)(65536*-0.08131+0.5)*B;
    }
}

void jpegenc::prepare_quant_tables()
{
    double aanscalefactor[8] = {
        1.0, 1.387039845, 1.306562965, 1.175875602,
        1.0, 0.785694958, 0.541196100, 0.275899379};
    BYTE row, col;
    BYTE i = 0;
    for (row = 0; row < 8; row++)
    {
        for (col = 0; col < 8; col++)
        {
            fdtbl_Y[i] = (float) (1.0 / ((double) DQTinfo.Ytable[zigzag[i]] *
                aanscalefactor[row] * aanscalefactor[col] * 8.0));
            fdtbl_Cb[i] = (float) (1.0 / ((double) DQTinfo.Cbtable[zigzag[i]] *
                aanscalefactor[row] * aanscalefactor[col] * 8.0));
            i++;
        }
    }
}

void jpegenc::fdct_and_quantization(SBYTE *data, float *fdtbl, SWORD *outdata)
{
    float tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    float tmp10, tmp11, tmp12, tmp13;
    float z1, z2, z3, z4, z5, z11, z13;
    float *dataptr;
    float datafloat[64];
    float temp;
    SBYTE ctr;
    BYTE i;
    for (i=0; i<64; i++) datafloat[i] = data[i];
    dataptr = datafloat;
    for (ctr = 7; ctr >= 0; ctr--) 
    {
        tmp0 = dataptr[0] + dataptr[7];
        tmp7 = dataptr[0] - dataptr[7];
        tmp1 = dataptr[1] + dataptr[6];
        tmp6 = dataptr[1] - dataptr[6];
        tmp2 = dataptr[2] + dataptr[5];
        tmp5 = dataptr[2] - dataptr[5];
        tmp3 = dataptr[3] + dataptr[4];
        tmp4 = dataptr[3] - dataptr[4];
        tmp10 = tmp0 + tmp3;
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;
        dataptr[0] = tmp10 + tmp11;
        dataptr[4] = tmp10 - tmp11;
        z1 = (tmp12 + tmp13) * ((float) 0.707106781);
        dataptr[2] = tmp13 + z1;
        dataptr[6] = tmp13 - z1;
        tmp10 = tmp4 + tmp5;
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;
        z5 = (tmp10 - tmp12) * ((float) 0.382683433);
        z2 = ((float) 0.541196100) * tmp10 + z5;
        z4 = ((float) 1.306562965) * tmp12 + z5;
        z3 = tmp11 * ((float) 0.707106781);
        z11 = tmp7 + z3;
        z13 = tmp7 - z3;
        dataptr[5] = z13 + z2;
        dataptr[3] = z13 - z2;
        dataptr[1] = z11 + z4;
        dataptr[7] = z11 - z4;
        dataptr += 8;
    }
    dataptr = datafloat;
    for (ctr = 7; ctr >= 0; ctr--) 
    {
        tmp0 = dataptr[0] + dataptr[56];
        tmp7 = dataptr[0] - dataptr[56];
        tmp1 = dataptr[8] + dataptr[48];
        tmp6 = dataptr[8] - dataptr[48];
        tmp2 = dataptr[16] + dataptr[40];
        tmp5 = dataptr[16] - dataptr[40];
        tmp3 = dataptr[24] + dataptr[32];
        tmp4 = dataptr[24] - dataptr[32];
        tmp10 = tmp0 + tmp3;
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;
        dataptr[0] = tmp10 + tmp11;
        dataptr[32] = tmp10 - tmp11;
        z1 = (tmp12 + tmp13) * ((float) 0.707106781);
        dataptr[16] = tmp13 + z1;
        dataptr[48] = tmp13 - z1;
        tmp10 = tmp4 + tmp5;
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;
        z5 = (tmp10 - tmp12) * ((float) 0.382683433);
        z2 = ((float) 0.541196100) * tmp10 + z5;
        z4 = ((float) 1.306562965) * tmp12 + z5;
        z3 = tmp11 * ((float) 0.707106781);
        z11 = tmp7 + z3;
        z13 = tmp7 - z3;
        dataptr[40] = z13 + z2;
        dataptr[24] = z13 - z2;
        dataptr[8] = z11 + z4;
        dataptr[56] = z11 - z4;
        dataptr++;
    }
    for (i = 0; i < 64; i++) 
    {
        temp = datafloat[i] * fdtbl[i];
        outdata[i] = (SWORD) ((SWORD)(temp + 16384.5) - 16384);
    }
}

void jpegenc::process_DU(SBYTE *ComponentDU,float *fdtbl,SWORD *DC, bitstring *HTDC,bitstring *HTAC)
{
    bitstring EOB = HTAC[0x00];
    bitstring M16zeroes = HTAC[0xF0];
    BYTE i;
    BYTE startpos;
    BYTE end0pos;
    BYTE nrzeroes;
    BYTE nrmarker;
    SWORD Diff;
    fdct_and_quantization(ComponentDU, fdtbl, DU_DCT);
    for (i=0; i<64; i++) DU[zigzag[i]]=DU_DCT[i];
    Diff = DU[0] - *DC;
    *DC = DU[0];
    if (Diff == 0) writebits(HTDC[0]);
    else 
    {
        writebits(HTDC[category[Diff]]);
        writebits(bitcode[Diff]);
    }
    for (end0pos=63; (end0pos>0)&&(DU[end0pos]==0); end0pos--) ;
    i = 1;
    while (i <= end0pos)
    {
        startpos = i;
        for (; (DU[i]==0) && (i<=end0pos); i++) ;
        nrzeroes = i - startpos;
        if (nrzeroes >= 16) 
        {
            for (nrmarker=1; nrmarker<=nrzeroes/16; nrmarker++) writebits(M16zeroes);
            nrzeroes = nrzeroes%16;
        }
        writebits(HTAC[nrzeroes*16+category[DU[i]]]);
        writebits(bitcode[DU[i]]);
        i++;
    }
    if (end0pos != 63) writebits(EOB);
}

void jpegenc::load_data_units_from_RGB_buffer(WORD xpos, WORD ypos)
{
    BYTE x, y;
    BYTE pos = 0;
    DWORD location;
    BYTE R, G, B;
    location = ypos * width + xpos;
    for (y=0; y<8; y++)
    {
        for (x=0; x<8; x++)
        {
            R = RGB_buffer[location].R;
            G = RGB_buffer[location].G;
            B = RGB_buffer[location].B;
            YDU[pos] = Y(R,G,B);
            CbDU[pos] = Cb(R,G,B);
            CrDU[pos] = Cr(R,G,B);
            location++;
            pos++;
        }
        location += width - 8;
    }
}

void jpegenc::main_encoder()
{
    SWORD DCY = 0, DCCb = 0, DCCr = 0;
    WORD xpos, ypos;
    for (ypos=0; ypos<height; ypos+=8)
    {
        for (xpos=0; xpos<width; xpos+=8)
        {
            load_data_units_from_RGB_buffer(xpos, ypos);
            process_DU(YDU, fdtbl_Y, &DCY, YDC_HT, YAC_HT);
            process_DU(CbDU, fdtbl_Cb, &DCCb, CbDC_HT, CbAC_HT);
            process_DU(CrDU, fdtbl_Cb, &DCCr, CbDC_HT, CbAC_HT);
        }
    }
}

int jpegenc::encode(const char *filename, colorRGB *pixels, int iw, int ih, int jpegquality)
{
    RGB_buffer = pixels;
    width =  iw;
    height = ih;    

    fp_jpeg_stream = fopen(filename,"wb");
    if(!fp_jpeg_stream)
    {
        //free(RGB_buffer);        
        free(category_alloc);
        free(bitcode_alloc);
        return -1;
    }

    bitstring fillbits;

    set_DQTinfo(jpegquality);
    set_DHTinfo();
    init_Huffman_tables();
    set_numbers_category_and_bitcode();
    precalculate_YCbCr_tables();
    prepare_quant_tables();
    
    SOF0info.width = width;
    SOF0info.height = height;

    writeword(0xFFD8);
    write_APP0info();
    // write_comment("Cris made this JPEG with his own encoder");
    write_DQTinfo();
    write_SOF0info();
    write_DHTinfo();
    write_SOSinfo();

    bytenew = 0; // current byte
    bytepos = 7; // bit position in this byte

    main_encoder();

    if (bytepos >= 0) 
    {
        fillbits.length = bytepos + 1;
        fillbits.value = (1<<(bytepos+1)) - 1;
        writebits(fillbits);
    }
    writeword(0xFFD9);

    //free(RGB_buffer);
    free(category_alloc);
    free(bitcode_alloc);
    fclose(fp_jpeg_stream);
    return 0;
};

#endif

