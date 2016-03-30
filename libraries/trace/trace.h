class Trace
{
    public:
        void log(char tag, int value);
        Trace(void) { index=0; n_entries=0; stopped=false; };
        ~Trace(void) { };
        void start() { index=0; n_entries=0; stopped=false;  };
        void stop()  { stopped=true; };
        void print();
		bool isStopped() { return stopped; }
    private:
        static const int SIZE = 1<<6;
        static const int MASK = SIZE-1;
        int index;
        int n_entries;
        unsigned long timestamps[SIZE];
        unsigned long rotcnt[SIZE];
        char tags[SIZE];
        int values[SIZE];
		bool stopped;
};

// trace object
extern Trace trace;