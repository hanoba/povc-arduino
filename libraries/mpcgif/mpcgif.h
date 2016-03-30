/*
 *  This is the header file interface to the gif.c file.
 *
 *  Requires that the app.h header be loaded first.
 */

/*
 *  Define USE_FILE_IO at compile-time.
 *    0 means: use GIF picture included in code
 *    1 means: use FILE I/O
 */
//#define SIMULATION
//void btWriteString(const char *textPtr);
// global constants & variables 
#define XSIZE 151     
#define YSIZE  40
#define MAXROW YSIZE // (YSIZE+20)
#define MAXCOL XSIZE //(XSIZE+49)

//#define LZ_MAX_CODE     4095    /*!< Largest 12 bit code */
//#define LZ_BITS         12
                                 
#define MAXDATA 256
#define COLORMAPSIZE 256
#define ROTATION_PERIOD_MS 50

// Default color map after init
#define COLORMASK  0xFF
#define BLACK  0
#define RED    1
#define YELLOW 2
#define GREEN  3
#define CYAN   4
#define BLUE   5
#define VIOLET 6
#define WHITE  7



/*
 *  Gif structures:
 */
#define Colour unsigned long

  
class GifDisplay
{
    public:
	    bool singleStepMode;
        GifDisplay(void) { //!< constructor
          thisPicture = &picture0;
          nextPicture = &picture1;
          init();
        }
        //!< set default colour map in 24-bit RGB format
        void init(void) {
      	    singleStepMode = false;
            gifScreen.has_cmap = 1;
            gifScreen.cmap.length = 8;
            gifScreen.cmap.colours[0] = 0x000000;   // Black
            gifScreen.cmap.colours[1] = 0xFF0000;   // Red
            gifScreen.cmap.colours[2] = 0xFFFF00;   // Yellow
            gifScreen.cmap.colours[3] = 0x00FF00;   // Green
            gifScreen.cmap.colours[4] = 0x00FFFF;   // Cyan
            gifScreen.cmap.colours[5] = 0x0000FF;   // Blue
            gifScreen.cmap.colours[6] = 0xFF00FF;   // Violet
            gifScreen.cmap.colours[7] = 0xFFFFFF;   // White
            thisPicture->has_cmap = 0;
            nextPicture->has_cmap = 0;
            thisPicture->colours = gifScreen.cmap.colours;
            nextPicture->colours = gifScreen.cmap.colours;
         }

        volatile int isNextPicturePending(void) { return nextPicture->is_pending; }; //!< return 1 if next GIF picture is ready
        
        void showGif(unsigned long length, const unsigned char *data);  //!< shows GIF file in memory
        void nextPictureTick(void);  //!< switches to next picture (to be called from ISR) - must only be called if nextPictureIsPending()==1

        inline Colour getThisPixelRGB(int x, int y) {  //!< returns pixel of current picture in RGB format (to be called by ISR)
//          return thisPicture->colours[thisPicture->data[x][y]]; 
            return thisPicture->has_cmap ? thisPicture->cmap.colours[thisPicture->data[x][y]] 
                                         : gifScreen.cmap.colours[thisPicture->data[x][y]]; 
        }
        inline unsigned char getThisPixel(int x, int y) {  //!< returns pixel of current picture in RGB format (to be called by ISR)
          return thisPicture->data[x][y]; 
        }
        inline void setThisPixel(int x, int y, unsigned char p) {  //!< sets pixel of current picture
          thisPicture->data[x][y] = p; }
		     
    private:
        //!< local constants
        static const int LZ_MAX_CODE    = 4095;    /*!< Largest 12 bit code */
        static const int LZ_BITS        = 12;

        static const int FLUSH_OUTPUT   = 4096;    /*!< Impossible code = flush */
        static const int FIRST_CODE     = 4097;    /*!< Impossible code = first */
        static const int NO_SUCH_CODE   = 4098;    /*!< Impossible code = empty */
                                 
        static const int HT_SIZE        = 8192;    /*!< 13 bit hash table size */
        static const int HT_KEY_MASK    = 0x1FFF;  /*!< 13 bit key mask */
                                 
        static const int IMAGE_LOADING  = 0;       /*!< file_state = processing */
        static const int IMAGE_SAVING   = 0;       /*!< file_state = processing */
        static const int IMAGE_COMPLETE = 1;       /*!< finished reading or writing */

        //!< Local data types
        typedef struct {
            int      length;
            Colour   colours[COLORMAPSIZE];
          } GifPalette;
        
        typedef struct {
            int          width, height;
            int          has_cmap, color_res, sorted, cmap_depth;
            int          bgcolour, aspect;
            GifPalette   cmap;
          } GifScreen;
        
        typedef struct {
            int             byte_count;
            unsigned char * bytes;
          } GifData;
        
        typedef struct {
            int        marker;
            int        data_count;
            //GifData ** data;
            GifData *  data[2];
          } GifExtension;
        
        typedef struct {
            int is_pending;   // added by HBA
            int left, top, width, height;
            int has_cmap, interlace, sorted, reserved, cmap_depth;
            GifPalette cmap;
            Colour *colours;
            // Graphics control extension (added by HBA)
            int user_flag;
            int disposal_method;
            int delay_ms;
            int transp_index;    
            unsigned char    data[MAXCOL][MAXROW];
          } GifPicture;
        
        typedef struct {
            int            intro;
            GifPicture *   pic;
            // GifExtension * ext;   HBA extension is stored in pic!
          } GifBlock;
        
        typedef struct {
            int depth,
                clear_code, eof_code,
                running_code, running_bits,
                max_code_plus_one,
                prev_code, current_code,
                stack_ptr,
                shift_state;
            unsigned long shift_data;
            unsigned long pixel_count;
            int           file_state, position, bufsize;
            unsigned char buf[256];
            unsigned char stack[LZ_MAX_CODE+1];
            unsigned char suffix[LZ_MAX_CODE+1];
            unsigned int  prefix[LZ_MAX_CODE+1];
          } GifDecoder;



        char        header[8];
        //GifScreen * screen;
        int         block_count;
        volatile GifPicture *thisPicture;     //!< includes frame buffer which is output by ISR
        volatile GifPicture *nextPicture;     //!< includes next picture to be output. pointers are swapped by ISR
        volatile GifPicture picture0;
        volatile GifPicture picture1;
        GifScreen gifScreen;
        
        //!> working buffers (were in original code allocated with malloc)
        GifBlock block;
        GifBlock gifblock;
        GifDecoder gifdecoder;
        unsigned char gifdata_bytes[MAXDATA];
        GifData gifdata;


        unsigned long gifFileDataLen; //!< size of GIF file in memory
        const unsigned char *gifFileData;  //!< pointer of GIF file in memory
        unsigned long gifFileIndex;  //!< index of next byte to be read

        unsigned long mem_read(void *ptr, unsigned long size, unsigned long count); //!< read data block from GIF file in memory
        int mem_getc(void); //!< read byte from GIF file in memory
        //void * gif_alloc(long bytes);
        
        int 	read_gif_int(void);
        
        GifData * new_gif_data(int size);
        GifData * read_gif_data(void);
        void	del_gif_data(GifData *data);
        void	write_gif_data(GifData *data);
        void	print_gif_data(GifData *data);
        
        void	read_gif_palette(GifPalette *cmap);
        //void	write_gif_palette(FILE *file, GifPalette *cmap);
        //void	print_gif_palette(FILE *file, GifPalette *cmap);
        
        GifScreen * new_gif_screen(void);
        void	del_gif_screen(GifScreen *screen);
        void	read_gif_screen(GifScreen *screen);
        void	write_gif_screen(GifScreen *screen);
        void	print_gif_screen(GifScreen *screen);
        
        GifExtension *new_gif_extension(void);
        void	read_gif_extension(GifPicture *pic);
        
        GifDecoder * new_gif_decoder(void);
        void	del_gif_decoder(GifDecoder *decoder);
        void	init_gif_decoder(GifDecoder *decoder);
        
        int	read_gif_code(GifDecoder *decoder);
        void	read_gif_line(GifDecoder *decoder, unsigned char *line, int length);
        
        GifPicture * new_gif_picture(void);
        void	del_gif_picture(GifPicture *pic);
        void	read_gif_picture(GifPicture *pic);
        void	write_gif_picture(GifPicture *pic);
        void	print_gif_picture(GifPicture *pic);
        
        GifBlock *new_gif_block(void);
        void	del_gif_block(GifBlock *block);
        void	read_gif_block(GifBlock *block);
        void	write_gif_block(GifBlock *block);
        void	print_gif_block(GifBlock *block);
        
        //Gif *	new_gif(void);
        //void	del_gif(Gif *gif);
        //void	read_gif(Gif *gif);
        void	read_one_gif_picture(void);
        //void	write_gif(FILE *file, Gif *gif);
        
        //Gif *	read_gif_file(const char *filename);
        //void	write_gif_file(const char *filename, Gif *gif);
        
        Colour rgb(unsigned char r, unsigned char g, unsigned char b);
        
        //void    print_gif(char *filename, Gif *gif);
        void    render_gif(void);
        
        void isr_simulation();
        void render_gif_picture_data();
        unsigned char read_byte();
        int read_stream(unsigned char*, int);
        unsigned char read_gif_byte(GifDecoder*);
        void finish_gif_picture(GifDecoder*);
        int trace_prefix(unsigned int*, int, int);
        void read_gif_picture_data(GifPicture*);
		void error(const char *errmsg);
};





  


