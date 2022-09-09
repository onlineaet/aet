

#ifndef __A_ASSERT_H__
#define __A_ASSERT_H__

#include <objectlib.h>

package$ aet.lang;

#define a_assert(expr)                  A_STMT_START { \
                                             if A_LIKELY (expr) ; else \
                                             AAssert.message(__FILE__, __LINE__, A_STRFUNC,#expr); \
                                        } A_STMT_END

#define a_assert_no_error(err)          A_STMT_START { \
                                             if (err) \
                                             AAssert.message(__FILE__, __LINE__, A_STRFUNC, \
                                                 #err, err, 0, 0); \
                                        } A_STMT_END

#define a_assert_error(err, dom, c)     A_STMT_START { \
                                               if (!err || (err)->domain != dom || (err)->code != c) \
                                               AAssert.message (__FILE__, __LINE__, A_STRFUNC, \
                                                 #err, err, dom, c); \
                                        } A_STMT_END

#define a_assert_cmpint(n1, cmp, n2)    A_STMT_START { \
                                             aint64 __n1 = (n1), __n2 = (n2); \
                                             if (__n1 cmp __n2) ; else \
                                             AAssert.message (__FILE__, __LINE__, A_STRFUNC, \
                                                 #n1 " " #cmp " " #n2, (long double) __n1, #cmp, (long double) __n2, 'i'); \
                                        } A_STMT_END
/**
 * 如果执行到这个语句，它会调用abort()退出程序
 * 断言函数a_assert_not_reached() 用来标识“不可能”的情况，通常用来检测不能处理的状况。
 * 比如:
 * switch (enum){
 *   case INT_TYPE:
 *     break ;
 *   default:
 *  /* 无效枚举值* /
 *      a_assert_not_reached();
 *     break ;
 *  }
 */
#define a_assert_not_reached()          A_STMT_START { AAssert.message (__FILE__, __LINE__, A_STRFUNC, NULL); } A_STMT_END

public$ class$ AAssert{
   public$  static void message (const char *file,int line,const char *func,const char *message);
   public$  static void message(const char *file,int line,const char *func,const char *expr, const AError *error,auint domain,int  code);
   public$  static void log(int type,const char *string1,const char *string2,int n_args,long double *largs);
   public$  static void message(const char *file,int line, const char *func,const char *expr,long double arg1,
                                  const char *cmp,long double arg2,char numtype);
   private$ static char *assertMsg=NULL;
   private$ static aboolean inSubprocess=FALSE;
};

#endif /* __G_QSORT_H__ */
