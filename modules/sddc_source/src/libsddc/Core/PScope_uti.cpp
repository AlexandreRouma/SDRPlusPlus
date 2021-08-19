#include "PScope_uti.h"
#include "license.txt" 

int PScopeShot(const char * filename, const char * title2, const char * title1, short * data, float samplerate, unsigned int numsamples )
{
    FILE *fp;
    fp = fopen(filename, "w+");
    fputs("Version,115\n", fp);
    fprintf(fp, "Retainers,0,1,%d,1024,0,%f,1,1\n",numsamples,samplerate );
    fputs("Placement,44,0,1,-1,-1,-1,-1,88,40,1116,879", fp);
    fputs("WindMgr,7,2,0\n", fp);
    fputs("Page,0,2\n", fp);
    fputs("Col,3,1\n", fp);
    fputs("Row,2,1\n", fp);
    fputs("Row,3,146\n", fp);
    fputs("Row,1,319\n", fp);
    fputs("Col,2,1063\n", fp);
    fputs("Row,4,1\n", fp);
    fputs("Row,0,319\n", fp);
    fputs("Page,1,2\n", fp);
    fputs("Col,1,1\n", fp);
    fputs("Row,1,1\n", fp);
    fputs("Col,2,425\n", fp);
    fputs("Row,4,1\n", fp);
    fputs("Row,0,319\n", fp);
    fprintf(fp,"DemoID,%s,%s,0\n", title1, title2 );
    fprintf(fp,"RawData,1,%d,16,-32768,32767,%f,-3.276800e+04,3.276800e+04\n", numsamples,samplerate);
    for (unsigned int n = 0; n < numsamples; n++ )
    {
      fprintf(fp, "%d\n", data[n]);
    }
    fputs("end\n", fp);
    return fclose(fp);
}

