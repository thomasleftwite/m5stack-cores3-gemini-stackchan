#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <float.h>
#ifndef __FLT_EPSILON__
#define __FLT_EPSILON__ FLT_EPSILON
#endif
// Host numerical stand-in for the ESP-DSP real FFT. Device builds use ESP-DSP.
inline int dsps_fft4r_initialized=0;
constexpr int ESP_OK=0;
inline bool dsp_is_power_of_two(int n) { return n>0 && !(n&(n-1)); }
inline int dsps_fft4r_init_fc32(void*,int) { dsps_fft4r_initialized=1; return 0; }
inline void dsps_fft4r_deinit_fc32() { dsps_fft4r_initialized=0; }
inline int dsps_fft4r_fc32(float*,int) { return 0; }
inline int dsps_bit_rev4r_fc32(float*,int) { return 0; }
inline int dsps_cplx2real_fc32(float* data,int half) {
    unsigned size=unsigned(half*2);
    std::vector<float> re(data,data+size),im(size,0);
    for(unsigned i=1,j=0;i<size;++i) {
        unsigned bit=size>>1; for(;j&bit;bit>>=1) j^=bit; j^=bit;
        if(i<j) std::swap(re[i],re[j]);
    }
    for(unsigned length=2;length<=size;length<<=1) {
        float angle=-6.283185307f/length,stepR=cosf(angle),stepI=sinf(angle);
        for(unsigned start=0;start<size;start+=length) {
            float wr=1,wi=0;
            for(unsigned j=0;j<length/2;++j) {
                unsigned a=start+j,b=a+length/2;
                float tr=wr*re[b]-wi*im[b],ti=wr*im[b]+wi*re[b];
                re[b]=re[a]-tr; im[b]=im[a]-ti; re[a]+=tr; im[a]+=ti;
                float next=wr*stepR-wi*stepI; wi=wr*stepI+wi*stepR; wr=next;
            }
        }
    }
    for(unsigned i=0;i<unsigned(half);++i) { data[2*i]=re[i]; data[2*i+1]=im[i]; }
    // ESP-DSP packs the real Nyquist bin into the imaginary slot of DC.
    data[1]=re[unsigned(half)];
    return 0;
}
inline int dsps_fft4r_fc32_ansi(float* p,int n) { return dsps_fft4r_fc32(p,n); }
inline int dsps_bit_rev4r_direct_fc32_ansi(float* p,int n) { return dsps_bit_rev4r_fc32(p,n); }
inline int dsps_cplx2real_fc32_ansi(float* p,int n) { return dsps_cplx2real_fc32(p,n); }
