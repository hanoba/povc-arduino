void btInit(uint32_t baudrate);
void btWriteString(char const *textPtr);
void btWriteChar(char ch);
char btReadChar(void);
int btReadString(char *cp, int nmax);
void btReadData(uint8_t *cp, int len);
int btCharAvailable(void);
