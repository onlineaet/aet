

#ifndef __AET_UTIL_A_RANDOM_H__
#define __AET_UTIL_A_RANDOM_H__

#include <objectlib.h>

package$ aet.util;

#define A_RAND_COUNT 624

/**
 *产生伪随机数的方法有很多种，比如线性同余法，
 *平方取中法等等，这里用的是梅森旋转算法
 *常见的两种为基于32位的MT19937-32和基于64位的MT19937-64
 *现在一般使用的是产生器有2^19937-1长的周期，而且在1-623维均匀分布的（映射到一维直线均匀分布，二维平面内均匀分布，三维空间内均匀分布...）。
 */

public$ class$ ARandom{
	public$ static ARandom *getInstance();
	private$ auint32 mt[A_RAND_COUNT]; /* 存储状态  */
	private$ auint mti;
	public$ ARandom(const auint32 *seed,auint length);
	public$ void setSeed (auint32  seed);
	public$ void setSeed(const auint32 *seed,auint length);
	public$ aint32 nextInt(aint32  begin,aint32  end);
	public$ auint32 nextInt();
	public$ adouble nextDouble();
	public$ adouble nextDouble (adouble  begin,adouble  end);

};




#endif /* __N_MEM_H__ */

