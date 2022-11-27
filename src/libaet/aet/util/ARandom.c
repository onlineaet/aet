#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "ARandom.h"
#include "AMutex.h"
#include "../lang/System.h"

/**
 *产生伪随机数的方法有很多种，比如线性同余法，
 *平方取中法等等，这里用的是梅森旋转算法
 *常见的两种为基于32位的MT19937-32和基于64位的MT19937-64
 *现在一般使用的是产生器有2^19937-1长的周期，而且在1-623维均匀分布的（映射到一维直线均匀分布，二维平面内均匀分布，三维空间内均匀分布...）。
 */

#define M 397
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */

/* Tempering parameters */
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)
#define A_USEC_PER_SEC 1000000

#define RAND_DOUBLE_TRANSFORM 2.3283064365386962890625e-10

static void timeSeed( auint32 seed[4])
{
  static aboolean dev_urandom_exists = TRUE;
  if (dev_urandom_exists){
      FILE* dev_urandom;
      do{
        dev_urandom = fopen("/dev/urandom", "rb");
      }while A_UNLIKELY (dev_urandom == NULL && errno == EINTR);

      if (dev_urandom){
            int r;
            setvbuf (dev_urandom, NULL, _IONBF, 0);
            do{
              errno = 0;
              r = fread (seed, sizeof ((auint32*)seed), 1, dev_urandom);
            }while A_UNLIKELY (errno == EINTR);
            if (r != 1)
              dev_urandom_exists = FALSE;
            fclose (dev_urandom);
      }else
        dev_urandom_exists = FALSE;
  }
  /**取时间作种子*/
  if (!dev_urandom_exists){
      aint64 now_us =System.currentTime();
      seed[0] = now_us / A_USEC_PER_SEC;
      seed[1] = now_us % A_USEC_PER_SEC;
      seed[2] = getpid ();
      seed[3] = getppid ();
  }
}



class$ GlobalRandom extends$ ARandom{
	AMutex lock;
};

impl$ ARandom{
    void createSeed (auint32  seed){
          mt[0]= seed;
          for (mti=1; mti<A_RAND_COUNT; mti++)
             mt[mti] = 1812433253UL *  (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti;
    }

	void createSeed(const auint32 *seed,auint length){
	   auint i, j, k;
	   a_return_if_fail (length >= 1);
	   createSeed (19650218UL);
	   i=1; j=0;
	   k = (A_RAND_COUNT>length ? A_RAND_COUNT : length);
	   for (; k; k--){
	      mt[i] = (mt[i] ^((mt[i-1] ^ (mt[i-1] >> 30)) * 1664525UL))+ seed[j] + j; /* non linear */
	      mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
	      i++; j++;
	      if (i>=A_RAND_COUNT){
		    mt[0] = mt[A_RAND_COUNT-1];
		    i=1;
		  }
	      if (j>=length)
		    j=0;
	   }
	   for (k=A_RAND_COUNT-1; k; k--){
		  mt[i] = (mt[i] ^((mt[i-1] ^ (mt[i-1] >> 30)) * 1566083941UL))- i; /* non linear */
		  mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
	      i++;
	      if (i>=A_RAND_COUNT){
	    	  mt[0] = mt[A_RAND_COUNT-1];
		      i=1;
		   }
	   }
	   mt[0] = 0x80000000UL; /* MSB is 1; assuring non-zero initial array */
	}

    ARandom(){
      self->mti=0;
      auint32 seed[4];
      timeSeed(seed);
      createSeed(seed, 4);
    }

    ARandom(const auint32 seed){
        createSeed(seed);
    }



    ARandom(const auint32 *seed,auint length){
        createSeed(seed,length);
    }

	void setSeed (auint32  seed){
	    createSeed(seed);
	}

    void setSeed(const auint32 *seed,auint length){
        createSeed(seed,length);
    }

	auint32 innerNextInt(){
	  auint32 y;
	  static const auint32 mag01[2]={0x0, MATRIX_A};
	  /* mag01[x] = x * MATRIX_A  for x=0,1 */
	  if (mti >= A_RAND_COUNT) { /* generate N words at one time */
	    int kk;
	    for (kk = 0; kk < A_RAND_COUNT - M; kk++) {
	      y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
	      mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1];
	    }
	    for (; kk < A_RAND_COUNT - 1; kk++) {
	      y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
	      mt[kk] = mt[kk+(M-A_RAND_COUNT)] ^ (y >> 1) ^ mag01[y & 0x1];
	    }
	    y = (mt[A_RAND_COUNT-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
	    mt[A_RAND_COUNT-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1];

	    mti = 0;
	  }

	  y = mt[mti++];
	  y ^= TEMPERING_SHIFT_U(y);
	  y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
	  y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
	  y ^= TEMPERING_SHIFT_L(y);
	  return y;
	}

    auint32 nextInt(){
        return innerNextInt();
    }

	aint32 innerNextInt(aint32  begin,aint32  end){
	  auint32 dist = end - begin;
	  auint32 random = 0;
	  a_return_val_if_fail (end > begin, begin);
      if (dist == 0)
		random = 0;
	  else{
		  /* maxvalue is set to the predecessor of the greatest
		   * multiple of dist less or equal 2^32.
		   */
		  auint32 maxvalue;
		  if (dist <= 0x80000000u){ /* 2^31 */

		      /* maxvalue = 2^32 - 1 - (2^32 % dist) */
		      auint32 leftover = (0x80000000u % dist) * 2;
		      if (leftover >= dist)
		    	  leftover -= dist;
		      maxvalue = 0xffffffffu - leftover;
		  }else
		      maxvalue = dist - 1;
		  do
		    random = innerNextInt();
		  while (random > maxvalue);
		  random %= dist;
	  }
	  return begin + random;
	}

	/* transform [0..2^32] -> [0..1] */
	adouble innerNextDouble(){
	  /* We set all 52 bits after the point for this, not only the first
	     32. That's why we need two calls to g_rand_int */
	  adouble retval = nextInt () * RAND_DOUBLE_TRANSFORM;
	  retval = (retval + nextInt()) * RAND_DOUBLE_TRANSFORM;
	  /* The following might happen due to very bad rounding luck, but
	   * actually this should be more than rare, we just try again then */
	  if (retval >= 1.0)
	    return innerNextDouble ();
	  return retval;
	}

    adouble nextDouble(){
        return innerNextDouble();
    }

    aint32 nextInt(aint32  begin,aint32  end){
        return innerNextInt(begin,end);
    }

    adouble innerNextDouble (adouble  begin,adouble  end){
          adouble r;
          r = innerNextDouble();
          return r * end - (r - 1) * begin;
    }

    adouble nextDouble (adouble  begin,adouble  end){
       return innerNextDouble(begin,end);
    }

	static ARandom *getInstance(){
	   static ARandom *singleton = NULL;
		if (!singleton){
			 singleton =new$ GlobalRandom();
		}
		return singleton;
	}
};

impl$ GlobalRandom {

	GlobalRandom(){
	    lock=new$ AMutex();
	}

	void setSeed (auint32 seed){
		lock.lock();
		super$->setSeed(seed);
		lock.unlock();
	}

	void setSeed(const auint32 *seed,auint length){
		lock.lock();
		super$->setSeed(seed,length);
		lock.unlock();
	}

	aint32 nextInt(aint32  begin,aint32 end){
		aint32 value;
		lock.lock();
		value=super$->nextInt(begin,end);
		lock.unlock();
		return value;
	}

	auint32 nextInt(){
		auint32 value;
		lock.lock();
		value=super$->nextInt();
		lock.unlock();
		return value;
	}

	adouble nextDouble(adouble  begin,adouble  end){
		adouble value;
		lock.lock();
		value=super$->nextDouble(begin,end);
		lock.unlock();
		return value;
	}

	adouble nextDouble(){
		adouble value;
		lock.lock();
		value=super$->nextDouble();
		lock.unlock();
		return value;
	}
};


