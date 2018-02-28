#include <fstream>
#include <math.h>
#include <string>
#include <string.h>
#include <iostream>
#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <iterator>
using namespace std;

// const int flow_num = 1000000;
// const int package_num = 32000000;

vector<string> vec;
vector<string> vec_flow;


long double alpha;
long double zetan;
long double eta;

unordered_map<string, int> mp;
unordered_map<string, int> mp_caida;

uint32_t rand_int() {
    union {
        uint32_t i;
        uint8_t c[4];
    } ret;

    for (int i = 0; i < 4; ++i) {
        ret.c[i] = uint8_t(rand() % 256);
    }

    return ret.i;
}

long double zeta(int n, long double skew) {
    long double res = 0;
    for (int i = 1; i <= n; i++) {
        res += pow(1.0 / i, skew);
    }
    return res;
}


// We borrow this code from https://answers.launchpad.net/polygraph/+faq/1478. 
// The corresponding paper is http://ldc.usb.ve/~mcuriel/Cursos/WC/spe2003.pdf.gz
int popzipf(int n, long double skew) {
    // popZipf(skew) = wss + 1 - floor((wss + 1) ** (x ** skew))
    long double u = rand() / (long double) (RAND_MAX);
    return (int) (n + 1 - floor(pow(n + 1, pow(u, skew))));
}

int unif(int n) {
    return rand() % n + 1;
}



int main() 
{
    srand(time(NULL));
    char filename_FlowTraffic[100];
    strcpy(filename_FlowTraffic, "./traffic_sender.dat");

    FILE *file_caida = fopen(filename_FlowTraffic, "r");
    int cnt = 0;
    
    uint sip, dip;
    unsigned short int sport, dport;
    unsigned short int type;

    char buf[600];
    fgets(buf, 500, file_caida);

    // while(fscanf(file_caida, "%u,%u,%hu,%hu,%hu", &sip, &dip, &sport, &dport, &type) != EOF)
    while(fgets(buf, 300, file_caida) != NULL)
    {
        mp_caida[string(buf)] ++;
        cnt++;
    }
    fclose(file_caida);

    printf("dataset: %s\n", filename_FlowTraffic);
    printf("total stream size = %d\n", cnt);
    printf("distinct item number = %d\n", mp_caida.size());

    int max_freq = 0;
    unordered_map<string, int>::iterator it = mp_caida.begin();

    for(int i = 0; i < mp_caida.size(); i++, it++)
    {
        vec_flow.push_back(it->first);
        int temp2 = it->second;
        max_freq = max_freq > temp2 ? max_freq : temp2;
    }
    printf("max_freq = %d\n", max_freq);




    int package_num = cnt;
    int flow_num = mp_caida.size();


    char filename[100];
    for (int ii = 0; ii <= 30; ii += 3) 
    {
        long double skew = ii * 1.0 / 10;

        alpha = 1 / (1 - skew);
        zetan = zeta(flow_num, skew);
        eta = (1 - pow(2.0 / flow_num, 1 - skew)) / (1 - zeta(2, skew) / zetan);

        mp.clear();
        vec.clear();
        sprintf(filename, "../traffic/zipf%03d.dat", ii);


        int next_value;

        for (int i = 0; i < package_num; i++) 
        {
            if (ii == 0)
                next_value = unif(flow_num);
            else
                next_value = popzipf(flow_num, skew);

            vec.push_back((vec_flow[next_value - 1]));
            mp[(vec_flow[next_value - 1])]++;
            if (i % (package_num / 20) == 0) 
            {
                printf(".");
                fflush(stdout);
            }
        }

        int max_val = 0, val;
        for (unordered_map<string, int>::iterator it = mp.begin(); it != mp.end(); it++) 
        {
            val = it->second;
            max_val = max_val > val ? max_val : val;
        }

        random_shuffle(vec.begin(), vec.end());
        int len = vec.size();
        printf("skew = %Lf, max_val = %d, flow_num = %d, package_num = %d\n", skew, max_val, (int) mp.size(), len);



        FILE * file_FlowTraffic = fopen(filename, "w");
        for(int i = 0; i < package_num; i++)
        {
            fprintf(file_FlowTraffic, "%s", vec[i].c_str());
        }
        fclose(file_FlowTraffic);
    }
    return 0;
}
