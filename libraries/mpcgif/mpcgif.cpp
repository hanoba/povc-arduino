/************************************************************************/
/* Display GIF file located in memory on POV Cylinder                   */
/************************************************************************/
/*!
*  This code has been derived from the cross platform GIF source code
*  (Version 3.6) from L. Patrick.
*
*
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
//#include <assert.h>

#include "mpcgif.h"
#include "trace.h"

/************************************************************************/
/* Host simulation under CYGWIN                                         */
/************************************************************************/
#ifdef SIMULATION
#   include <unistd.h>
#   include "xwin.h"
#   define delay(x)
#   define btWriteString(x) printf(x)
#   define printInfo(x)
#else
#   include "Arduino.h"
#   include "bt.h"
#   define xShowFrameBuffer(x)
#   define xAllocateColorMap(x)
	extern void printInfo(int repeatFlag);
#endif

/************************************************************************/
/* Macro for debug prints                                               */
/************************************************************************/
volatile int tickc;
//#define DEBUGGING
#ifdef DEBUGGING
#   define DEBUG_PRINT(fmt, x) debug_print(__FUNCTION__, __LINE__, fmt, x)
void debug_print(const char *function, int line, const char *format, int value)
{
	const int TEXTLEN = 80;
	char text[TEXTLEN];
	snprintf(text, TEXTLEN, "Function %s Line %d: ", function, line);
	//btWriteString(text);
	snprintf(text, TEXTLEN, format, value);
	btWriteString(text);
}
#else
#   define DEBUG_PRINT(fmt, x)
#endif

//#pragma GCC optimize ("-O0")


//----------------------------------------------------------------------------------------
  void GifDisplay::error(const char *errmsg)
//----------------------------------------------------------------------------------------
{
	char text[80];
	snprintf(text, 80, "\n\n%s!\nSYSTEM HALTED!\n", errmsg); 
	btWriteString(text);
	while (1);
}

//----------------------------------------------------------------------------------------
  unsigned long GifDisplay::mem_read(void *ptr, unsigned long size, unsigned long count) {
//----------------------------------------------------------------------------------------
    unsigned char *p = (unsigned char *) ptr;
    unsigned long i;
    
    for (i=0; i<count*size; i++) {
        if (gifFileIndex >= gifFileDataLen) error("Error in mem_read");
        *p++ = gifFileData[gifFileIndex++];
    }
    return i/size;
}

//----------------------------------------------------------------------------------------
  int GifDisplay::mem_getc(void) {
//----------------------------------------------------------------------------------------
    unsigned char ch;
    
    mem_read(&ch, 1, 1);
    return (int) ch;
}


//!< This functions is called by the ISR
//----------------------------------------------------------------------------------------
    void GifDisplay::nextPictureTick(void)
//----------------------------------------------------------------------------------------
{
    volatile GifPicture *tmp;
    ++tickc;        
	//trace.log('T', tickc);
    if (thisPicture->delay_ms > 0) {
        thisPicture->delay_ms -= ROTATION_PERIOD_MS;
    }

    if (thisPicture->delay_ms <= 0 && isNextPicturePending()) {    
        thisPicture->is_pending = 0;
        tmp = thisPicture;
        thisPicture = nextPicture;
        nextPicture = tmp;
#ifdef SIMULATION
        if (thisPicture->has_cmap) {
            xAllocateColorMap(thisPicture->cmap.length, (unsigned long *)thisPicture->cmap.colours);
        } else {
            xAllocateColorMap(gifScreen.cmap.length, gifScreen.cmap.colours);
        }
        xShowFrameBuffer(this);
#endif        
    }
}


//----------------------------------------------------------------------------------------
    void GifDisplay::isr_simulation(void)
//----------------------------------------------------------------------------------------
{
#ifdef SIMULATION
        usleep(ROTATION_PERIOD_MS*1000);      // 50 ms corresponds to one rotation at 20Hz
        nextPictureTick();
#endif 
#if 0
        if (singleStepMode) {
			btWriteString("\nSingle Step Mode enabled!\nPress any key for next picture (':' to quit):");
			if (btReadChar() ==':') singleStepMode=false;
			btWriteString("\n");
		}		
#endif        
}

        
// Added by HBA: rendering
//----------------------------------------------------------------------------------------
  void GifDisplay::render_gif_picture_data(void)
//----------------------------------------------------------------------------------------
{
	const int DISTANCE = 5;
    int row, col;
    int left, top;
    int height, width;

#if 1
	int i, n_copies, cwidth;
   
    // replicate GIF
	width = gifScreen.width;
    cwidth = width+DISTANCE;
    n_copies = MAXCOL / cwidth;
    //printf("%d copies - width=%d\n", n_copies, width);
    for (i=1; i<n_copies; i++)
      for (row=0; row<YSIZE; row++)
        for (col=0; col<width; col++)
          nextPicture->data[col+i*cwidth][row] = nextPicture->data[col][row];
#endif        
    trace.log('R', nextPicture->delay_ms);
    nextPicture->is_pending = 1;
	while (isNextPicturePending()) {
        isr_simulation();
		delay(1);	//  1 ms
    }
    trace.log('r', 0);
	printInfo(0);

    height = thisPicture->height;
    width = thisPicture->width;
    left = thisPicture->left;
    top = thisPicture->top;
    // note: nextPicture contains the previously displayed picture
    if  (thisPicture->disposal_method==3) {
        // restore to previous (means: do not copy current)
        for (row = 0; row < YSIZE; row++) {
            for (col = 0; col < XSIZE; col++) {
                if (row<top  || row>=top+height || col<left || col>=left+width)
                    nextPicture->data[col][row] = thisPicture->data[col][row];
            }
        }
    }
    else memcpy((void *)&nextPicture->data, (void *)&thisPicture->data, MAXROW*MAXCOL);

    if (thisPicture->disposal_method==2) {
        // restore to background colour
         for (row = 0; row < height; row++) {
             for (col = 0; col < width; col++) {
                  nextPicture->data[col+left][row+top] = gifScreen.bgcolour;
             }
         }
    }
	return;
}


// added by HBA - Format for LED strip is BRG not RGB (and not GRB)
//----------------------------------------------------------------------------------------
  Colour GifDisplay::rgb(unsigned char r, unsigned char g, unsigned char b) {
//----------------------------------------------------------------------------------------
    // round colors to 7 bits
    r = (r==0xFF ? 0xFF : r+1) & 0xFE;
    b = (b==0xFF ? 0xFF : b+1) & 0xFE;
    g = (g==0xFF ? 0xFF : g+1) & 0xFE;
    return (Colour) r << 7  |
           (Colour) g >> 1  | 
           (Colour) b << 15; 
}



/*
 *  GIF file input/output functions.
 */

//----------------------------------------------------------------------------------------
  unsigned char GifDisplay::read_byte(void)
//----------------------------------------------------------------------------------------
{
    int ch = mem_getc();
    if (ch == EOF)
        ch = 0;
    return ch;
}

//----------------------------------------------------------------------------------------
  int GifDisplay::read_stream(unsigned char buffer[], int length)
//----------------------------------------------------------------------------------------
{
    int count = (int) mem_read(buffer, 1, length);
    int i = count;
    while (i < length)
        buffer[i++] = '\0';
    return count;
}

//----------------------------------------------------------------------------------------
  int GifDisplay::read_gif_int(void)
//----------------------------------------------------------------------------------------
{
    int output;
    unsigned char buf[2];

    if (mem_read(buf, 1, 2) != 2) error("Error in read_gif_int");
    output = (((unsigned int) buf[1]) << 8) | buf[0];
    return output;
}

/*
 *  Gif data blocks:
 */

//----------------------------------------------------------------------------------------
  GifDisplay::GifData * GifDisplay::new_gif_data(int size)
//----------------------------------------------------------------------------------------
{
    GifData *data = &gifdata;

    memset (&gifdata_bytes, 0, MAXDATA);
    memset (&gifdata, 0, sizeof(GifData));
    //assert(size < MAXDATA);
	if (size >= MAXDATA) error("Error in new_gif_data");
    
    if (data) {
        data->byte_count = size;
        data->bytes = &gifdata_bytes[0];
    }
    return data;
}

/*
 *  Read one code block from the Gif file.
 *  This routine should be called until NULL is returned.
 *  Use app_free() to free the returned array of bytes.
 */
//----------------------------------------------------------------------------------------
  GifDisplay::GifData * GifDisplay::read_gif_data(void)
//----------------------------------------------------------------------------------------
{
    GifData *data;
    int size;

    size = read_byte();

    if (size > 0) {
        data = new_gif_data(size);
        read_stream(data->bytes, size);
    }
    else {
        data = NULL;
    }
    return data;
}


/*
 *  Read the next byte from a Gif file.
 *
 *  This function is aware of the block-nature of Gif files,
 *  and will automatically skip to the next block to find
 *  a new byte to read, or return 0 if there is no next block.
 */
//----------------------------------------------------------------------------------------
  unsigned char GifDisplay::read_gif_byte(GifDecoder *decoder)
//----------------------------------------------------------------------------------------
{
    unsigned char *buf = decoder->buf;
    unsigned char next;

    if (decoder->file_state == IMAGE_COMPLETE)
        return '\0';

    if (decoder->position == decoder->bufsize)
    {   /* internal buffer now empty! */
        /* read the block size */
        decoder->bufsize = read_byte();
        if (decoder->bufsize == 0) {
            decoder->file_state = IMAGE_COMPLETE;
            return '\0';
        }
        read_stream(buf, decoder->bufsize);
        next = buf[0];
        decoder->position = 1;  /* where to get chars */
    }
    else {
        next = buf[decoder->position++];
    }

    return next;
}

/*
 *  Read to end of an image, including the zero block.
 */
//----------------------------------------------------------------------------------------
  void GifDisplay::finish_gif_picture(GifDecoder *decoder)
//----------------------------------------------------------------------------------------
{
    unsigned char *buf = decoder->buf;

    while (decoder->bufsize != 0) {
        decoder->bufsize = read_byte();
        if (decoder->bufsize == 0) {
            decoder->file_state = IMAGE_COMPLETE;
            break;
        }
        read_stream(buf, decoder->bufsize);
    }
}


//----------------------------------------------------------------------------------------
  void GifDisplay::read_gif_palette(GifPalette *cmap)
//----------------------------------------------------------------------------------------
{
    int i;
    unsigned int r, g, b;

    // cmap->colours = ledColorMap; //app_alloc(cmap->length * sizeof(Colour));

    for (i=0; i<cmap->length; i++) {
        r = read_byte();
        g = read_byte();
        b = read_byte();
        //cmap->colours[i] = rgb(r,g,b);
        cmap->colours[i] = r<<16 | g<<8 | b;  
    }
    // xAllocateColorMap(cmap->colours, cmap->length);
}


//----------------------------------------------------------------------------------------
  void GifDisplay::read_gif_screen(GifScreen *screen)
//----------------------------------------------------------------------------------------
{
    unsigned char info;

    screen->width       = read_gif_int();
    screen->height      = read_gif_int();
	if (screen->width>MAXCOL || screen->height>MAXROW) error("Error: GifScreenSizeOutOfRange");

    info                = read_byte();
    screen->has_cmap    =  (info & 0x80) >> 7;
    screen->color_res   = ((info & 0x70) >> 4) + 1;
    screen->sorted      =  (info & 0x08) >> 3;
    screen->cmap_depth  =  (info & 0x07)       + 1;
	if (screen->cmap_depth>8) error("Error: GifCmapDepthOutOfRange");

    screen->bgcolour    = read_byte();
    screen->aspect      = read_byte();

    if (screen->has_cmap) {
        screen->cmap.length = 1 << screen->cmap_depth;
        read_gif_palette(&screen->cmap);
        //xxxxxxxxxxxxxxxxscreen->cmap.colours[screen->bgcolour] = 0; //HBA: Set Background colour to black!!!
    }
    memset((void *)nextPicture->data, screen->bgcolour, MAXCOL*MAXROW);
}


//----------------------------------------------------------------------------------------
  void GifDisplay::read_gif_extension(GifPicture *pic)
//----------------------------------------------------------------------------------------
{
    GifData *data;
    int marker;
    int transparencyFlag;

    marker = read_byte();

    data = read_gif_data();
    // added by HBA:
    if (marker==0xF9) {
        // extract graphics control extension parameter
        transparencyFlag     = data->bytes[0]>>0 & 1;
        pic->user_flag       = data->bytes[0]>>1 & 1;
        pic->disposal_method = data->bytes[0]>>2 & 0x07;
        pic->delay_ms        = 10*(data->bytes[1]+(data->bytes[2]<<8));
        pic->transp_index    = transparencyFlag ? data->bytes[3] : -1;
    }
    // ext->data_count = 1;
    // ext->data[0] = data;
    
    while (data) {
        /* Append the data object: */
        // i = ++ext->data_count;
        // ext->data = app_realloc(ext->data, i * sizeof(GifData *));
        // ext->data[i-1] = data;
        data = read_gif_data();
    }
}


/*
 *  GifDecoder:
 */

//----------------------------------------------------------------------------------------
  GifDisplay::GifDecoder * GifDisplay::new_gif_decoder(void)
//----------------------------------------------------------------------------------------
{
    memset (&gifdecoder, 0, sizeof(GifDecoder));
    return &gifdecoder;  // gif_alloc(sizeof(GifDecoder));
}

//----------------------------------------------------------------------------------------
  void GifDisplay::init_gif_decoder(GifDecoder *decoder)
//----------------------------------------------------------------------------------------
{
    int i, depth;
    int lzw_min;
    unsigned int *prefix;

    lzw_min = read_byte();
    depth = lzw_min;

    decoder->file_state   = IMAGE_LOADING;
    decoder->position     = 0;
    decoder->bufsize      = 0;
    decoder->buf[0]       = 0;
    decoder->depth        = depth;
    decoder->clear_code   = (1 << depth);
    decoder->eof_code     = decoder->clear_code + 1;
    decoder->running_code = decoder->eof_code + 1;
    decoder->running_bits = depth + 1;
    decoder->max_code_plus_one = 1 << decoder->running_bits;
    decoder->stack_ptr    = 0;
    decoder->prev_code    = NO_SUCH_CODE;
    decoder->shift_state  = 0;
    decoder->shift_data   = 0;

    prefix = decoder->prefix;
    for (i = 0; i <= LZ_MAX_CODE; i++)
        prefix[i] = NO_SUCH_CODE;
}

/*
 *  Read the next Gif code word from the file.
 *
 *  This function looks in the decoder to find out how many
 *  bits to read, and uses a buffer in the decoder to remember
 *  bits from the last byte input.
 */
int GifDisplay::read_gif_code(GifDecoder *decoder)
{
    int code;
    unsigned char next_byte;
    static int code_masks[] = {
        0x0000, 0x0001, 0x0003, 0x0007,
        0x000f, 0x001f, 0x003f, 0x007f,
        0x00ff, 0x01ff, 0x03ff, 0x07ff,
        0x0fff
    };

    while (decoder->shift_state < decoder->running_bits)
    {
        /* Need more bytes from input file for next code: */
        next_byte = read_gif_byte(decoder);
        decoder->shift_data |=
          ((unsigned long) next_byte) << decoder->shift_state;
        decoder->shift_state += 8;
    }

    code = decoder->shift_data & code_masks[decoder->running_bits];

    decoder->shift_data >>= decoder->running_bits;
    decoder->shift_state -= decoder->running_bits;

    /* If code cannot fit into running_bits bits,
     * we must raise its size.
     * Note: codes above 4095 are used for signalling. */
    if (++decoder->running_code > decoder->max_code_plus_one
        && decoder->running_bits < LZ_BITS)
    {
        decoder->max_code_plus_one <<= 1;
        decoder->running_bits++;
    }
    return code;
}

/*
 *  Routine to trace the prefix-linked-list until we get
 *  a prefix which is a pixel value (less than clear_code).
 *  Returns that pixel value.
 *
 *  If the picture is defective, we might loop here forever,
 *  so we limit the loops to the maximum possible if the
 *  picture is okay, i.e. LZ_MAX_CODE times.
 */
//----------------------------------------------------------------------------------------
  int GifDisplay::trace_prefix(unsigned int *prefix, int code, int clear_code)
//----------------------------------------------------------------------------------------
{
    int i = 0;

    while (code > clear_code && i++ <= LZ_MAX_CODE)
        code = prefix[code];
    return code;
}

/*
 *  The LZ decompression routine:
 *  Call this function once per scanline to fill in a picture.
 */
//----------------------------------------------------------------------------------------
  void GifDisplay::read_gif_line(GifDecoder *decoder, unsigned char *line, int length)
//----------------------------------------------------------------------------------------
{
    int i = 0, j;
    int current_code, eof_code, clear_code;
    int current_prefix, prev_code, stack_ptr;
    unsigned char *stack, *suffix;
    unsigned int *prefix;

    prefix  = decoder->prefix;
    suffix  = decoder->suffix;
    stack   = decoder->stack;
    stack_ptr   = decoder->stack_ptr;
    eof_code    = decoder->eof_code;
    clear_code  = decoder->clear_code;
    prev_code   = decoder->prev_code;

    if (stack_ptr != 0) {
        /* Pop the stack */
        while (stack_ptr != 0 && i < length) {
            line[i++] = stack[--stack_ptr];
        }    
    }

    while (i < length)
    {
    current_code = read_gif_code(decoder);

    if (current_code == eof_code)
    {
       /* unexpected EOF */
       if (i != length - 1 || decoder->pixel_count != 0) error("Error: Unexpected end of GIF file");
       i++;
    }
    else if (current_code == clear_code)
    {
        /* reset prefix table etc */
        for (j = 0; j <= LZ_MAX_CODE; j++)
        prefix[j] = NO_SUCH_CODE;
        decoder->running_code = decoder->eof_code + 1;
        decoder->running_bits = decoder->depth + 1;
        decoder->max_code_plus_one = 1 << decoder->running_bits;
        prev_code = decoder->prev_code = NO_SUCH_CODE;
    }
    else {
        /* Regular code - if in pixel range
         * simply add it to output pixel stream,
         * otherwise trace code-linked-list until
         * the prefix is in pixel range. */
        if (current_code < clear_code) {
            /* Simple case. */
            line[i++] = current_code;
        }
        else {
            /* This code needs to be traced:
             * trace the linked list until the prefix is a
             * pixel, while pushing the suffix pixels on
             * to the stack. If finished, pop the stack
             * to output the pixel values. */
            if ((current_code < 0) || (current_code > LZ_MAX_CODE))
                error("Error: Image defect"); /* image defect */
            if (prefix[current_code] == NO_SUCH_CODE) {
                /* Only allowed if current_code is exactly
                 * the running code:
                 * In that case current_code = XXXCode,
                 * current_code or the prefix code is the
                 * last code and the suffix char is
                 * exactly the prefix of last code! */
                if (current_code == decoder->running_code - 2) {
                current_prefix = prev_code;
                suffix[decoder->running_code - 2]
                    = stack[stack_ptr++]
                    = trace_prefix(prefix, prev_code, clear_code);
                }
                else {
                error("Error: Image defect"); /* image defect */
                }
            }
            else
                current_prefix = current_code;
            
            /* Now (if picture is okay) we should get
             * no NO_SUCH_CODE during the trace.
             * As we might loop forever (if picture defect)
             * we count the number of loops we trace and
             * stop if we get LZ_MAX_CODE.
             * Obviously we cannot loop more than that. */
            j = 0;
            while (j++ <= LZ_MAX_CODE
                && current_prefix > clear_code
                && current_prefix <= LZ_MAX_CODE)
            {
                stack[stack_ptr++] = suffix[current_prefix];
                current_prefix = prefix[current_prefix];
            }
            if (j >= LZ_MAX_CODE || current_prefix > LZ_MAX_CODE)
                error("Error: Image defect"); /* image defect */
            
            /* Push the last character on stack: */
            stack[stack_ptr++] = current_prefix;

            /* Now pop the entire stack into output: */
            while (stack_ptr != 0 && i < length) {
                line[i++] = stack[--stack_ptr];
            }
        }
        if (prev_code != NO_SUCH_CODE) {
        if ((decoder->running_code < 2) ||
          (decoder->running_code > LZ_MAX_CODE+2))
            error("Error: Image defect"); /* image defect */
        prefix[decoder->running_code - 2] = prev_code;

        if (current_code == decoder->running_code - 2) {
            /* Only allowed if current_code is exactly
             * the running code:
             * In that case current_code = XXXCode,
             * current_code or the prefix code is the
             * last code and the suffix char is
             * exactly the prefix of the last code! */
            suffix[decoder->running_code - 2]
            = trace_prefix(prefix, prev_code, clear_code);
        }
        else {
            suffix[decoder->running_code - 2]
            = trace_prefix(prefix, current_code, clear_code);
        }
        }
        prev_code = current_code;
    }
    }

    decoder->prev_code = prev_code;
    decoder->stack_ptr = stack_ptr;
}

/*
 *  Hash table:
 */

/*
 *  The 32 bits contain two parts: the key & code:
 *  The code is 12 bits since the algorithm is limited to 12 bits
 *  The key is a 12 bit prefix code + 8 bit new char = 20 bits.
 */
#define HT_GET_KEY(x)   ((x) >> 12)
#define HT_GET_CODE(x)  ((x) & 0x0FFF)
#define HT_PUT_KEY(x)   ((x) << 12)
#define HT_PUT_CODE(x)  ((x) & 0x0FFF)


//----------------------------------------------------------------------------------------
  void GifDisplay::read_gif_picture_data(GifPicture *pic)
//----------------------------------------------------------------------------------------
{
    GifDecoder *decoder; //*decoder;
    long w, h, top, left;
    int interlace_start[] = {0, 4, 2, 1};
    int interlace_step[]  = {8, 8, 4, 2};
    int scan_pass, row, ti;
    unsigned char line[MAXCOL];
    int i;

    w = pic->width;
    h = pic->height;
    top = pic->top;             // HBA
    left = pic->left;           // HBA
    ti = pic->transp_index;     // HBA
#if 0    
    pic->data = app_alloc(h * sizeof(unsigned char *));
    if (pic->data == NULL)
        return;
    for (row=0; row < h; row++)
        pic->data[row] = app_zero_alloc(w * sizeof(unsigned char));
#endif
    decoder = new_gif_decoder();
    init_gif_decoder(decoder);
    if (pic->interlace) {
      for (scan_pass = 0; scan_pass < 4; scan_pass++) {
        row = interlace_start[scan_pass];
        while (row < h) {
          read_gif_line(decoder, line, w);
          for (i=0; i<w; i++) if(line[i]!=ti) pic->data[left+i][row+top] = line[i];
          row += interlace_step[scan_pass];
        }
      }
    }
    else {
      row = 0;
      while (row < h) {
        read_gif_line(decoder, line, w);
        for (i=0; i<w; i++) if(line[i]!=ti) pic->data[left+i][row+top] = line[i];
        row += 1;
      }
    }

    finish_gif_picture(decoder);

    //del_gif_decoder(decoder);
    
    // Added by HBA - immediate rendering
    render_gif_picture_data();
}

//----------------------------------------------------------------------------------------
  void GifDisplay::read_gif_picture(GifPicture *pic)
//----------------------------------------------------------------------------------------
{
    unsigned char info;

    pic->left   = read_gif_int();
    pic->top    = read_gif_int();
    pic->width  = read_gif_int();
    pic->height = read_gif_int();

    info = read_byte();
    pic->has_cmap    = (info & 0x80) >> 7;
    pic->interlace   = (info & 0x40) >> 6;
    pic->sorted      = (info & 0x20) >> 5;
    pic->reserved    = (info & 0x18) >> 3;

    if (pic->has_cmap) {
        pic->cmap_depth  = (info & 0x07) + 1;
        pic->cmap.length = 1 << pic->cmap_depth;
        read_gif_palette(&pic->cmap);
        //pic->cmap.colours[gifScreen.bgcolour] = 0; // HBA: set background colour to black
        pic->colours = pic->cmap.colours;
    } else {
        pic->colours = gifScreen.cmap.colours;
    }


    read_gif_picture_data(pic);
}


//----------------------------------------------------------------------------------------
  void GifDisplay::read_one_gif_picture(void)
//----------------------------------------------------------------------------------------
{
    int i;
    GifBlock *block = &gifblock;

    for (i=0; i<6; i++)
        header[i] = read_byte();
    if (strncmp(header, "GIF", 3) != 0)
        error("Error: Wrong GIF header"); /* error */

    read_gif_screen(&gifScreen);

    while (1) {
        //block = new_gif_block();
        memset (block, 0, sizeof(GifBlock));
        read_gif_block(block);

        if (block->intro == 0x3B) { /* terminator */
            //del_gif_block(block);
            break;
        }
        else if (block->intro == 0x2C) { /* image */
            /* Append the block: */
            i = ++block_count;
            //gif->blocks = app_realloc(gif->blocks, i * sizeof(GifBlock *));
            //gif->blocks[i-1] = block;
            break;
        }
        else if (block->intro == 0x21) { /* extension */
            /* Append the block: */
            i = ++block_count;
            //gif->blocks = app_realloc(gif->blocks, i * sizeof(GifBlock *));
            //gif->blocks[i-1] = block;
            continue;
        }
        else {  /* error! */
            //del_gif_block(block);
            break;
        }
    }
}


//----------------------------------------------------------------------------------------
  void GifDisplay::read_gif_block(GifBlock *block)
//----------------------------------------------------------------------------------------
{
    block->intro = read_byte();
    if (block->intro == 0x2C) {
        block->pic = (GifPicture *) nextPicture; //new_gif_picture();
        read_gif_picture(block->pic);
    }
    else if (block->intro == 0x21) {
        // block->ext = new_gif_extension();
        // read_gif_extension(block->ext);
        block->pic = (GifPicture *) nextPicture; // new_gif_picture();
        read_gif_extension(block->pic);
    }
}


//----------------------------------------------------------------------------------------
    void GifDisplay::showGif(unsigned long length, const unsigned char *dataPtr)  //!< shows GIF file in memory
//----------------------------------------------------------------------------------------
{
    int i;
    
    gifFileDataLen = length;
    gifFileData = dataPtr;
    gifFileIndex = 0;
    
    //memset (&gif1, 0, sizeof(Gif));
    //strcpy(gif->header, "GIF87a");
    memset (&gifScreen, 0, sizeof(GifScreen));
    //screen = &gifScreen;

    for (i=0; i<6; i++)
        header[i] = read_byte();
    if (strncmp(header, "GIF", 3) != 0)
        error("Error: Wrong GIF header"); /* error */

    read_gif_screen(&gifScreen);
    while (1) {
        //block = new_gif_block();
        read_gif_block(&block);

        if (block.intro == 0x3B) { /* terminator */
            //del_gif_block(block);
            break;
        }
        else  if (block.intro == 0x2C) {   /* image */
            /* Append the block: */
            i = ++block_count;
            //gif->blocks = app_realloc(gif->blocks, i * sizeof(GifBlock *));
            //gif->blocks[i-1] = block;
        }
        else  if (block.intro == 0x21) {   /* extension */
            /* Append the block: */
            i = ++block_count;
            //gif->blocks = app_realloc(gif->blocks, i * sizeof(GifBlock *));
            //gif->blocks[i-1] = block;
        }
        else {  /* error */
            //del_gif_block(block);
            break;
        }
    }
}
