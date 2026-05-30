
AET_BASE=aet/natomic.o  aet/nmem.o  aet/nslice.o  aet/nlist.o \
    aet/nlog.o   aet/nqsort.o aet/nptrarray.o aet/nerror.o \
    aet/nprintf.o aet/nstrfuncs.o  aet/nunicode.o aet/nconvert.o \
    aet/nuniprop.o aet/nstring.o aet/nunidecomp.o aet/nhash.o \
    aet/nfileutils.o aet/ncharset.o aet/localcharset.o aet/nfile.o 

AET_TREE=aet/c-aet.o aet/classinfo.o aet/classmgr.o aet/aetutils.o aet/aetprinttree.o\
	 aet/aetmangle.o  aet/classparser.o aet/classimpl.o  aet/aetprinttoken.o aet/aetdiagnosticiface.o\
    aet/classref.o aet/classinit.o aet/varcall.o aet/classfinalize.o  aet/classctor.o\
    aet/supercall.o aet/funchelp.o aet/funccall.o  aet/classinterface.o aet/classcast.o \
    aet/classpackage.o aet/genericutil.o aet/genericcall.o  aet/classdot.o aet/classpermission.o\
    aet/varmgr.o aet/parserhelp.o aet/parserstatic.o aet/genericimpl.o aet/generic-typeck.o\
    aet/classutil.o aet/classfunc.o aet/funcmgr.o  aet/newobject.o aet/newstack.o\
    aet/newstrategy.o aet/newheap.o aet/newfield.o aet/genericblock.o aet/blockmgr.o\
    aet/genericinfo.o aet/aet-dump.o aet/genericquery.o  aet/genericmodel.o aet/genericparser.o\
    aet/classaccess.o aet/makefileparm.o  aet/middlefile.o  aet/classfinal.o aet/classbuild.o\
    aet/genericfunc.o  aet/objectreturn.o aet/enumparser.o aet/restorelocation.o\
    aet/namelesscall.o aet/implicitlycall.o aet/cmprefopt.o  aet/accesscontrols.o  aet/aetexpr.o\
    aet/clearwarning.o aet/selectfield.o aet/funcpointer.o aet/aetdiagnosticimpl.o aet/aetprintgimple.o\
    aet/mtcsparser.o aet/aetmediator.o aet/aetparser.o aet/mtcsbuiltintree.o  aet/mtcstypes.o\
    aet/aetcollect.o aet/mtcsinfo.o aet/mtcslanuch.o  aet/genericgraph.o aet/aetlib.o\
    aet/ifaceimpl.o aet/genericcode.o aet/mtcslink.o \

AET_MTCS=aet/mtcs/mtcsalign.o aet/mtcs/mtcsargs.o aet/mtcs/mtcsasm.o aet/mtcs/mtcscalls.o\
    aet/mtcs/mtcsccmp.o aet/mtcs/mtcscfgloopanal.o aet/mtcs/mtcscodes.o aet/mtcs/mtcscompile.o\
    aet/mtcs/mtcscomponent.o aet/mtcs/mtcsdojump.o aet/mtcs/mtcsdwarf2asm.o aet/mtcs/mtcsdwarf2cfi.o\
    aet/mtcs/mtcsdwarf2out.o aet/mtcs/mtcsemit.o aet/mtcs/mtcsexcept.o aet/mtcs/mtcsexpand.o\
    aet/mtcs/mtcsexplow.o aet/mtcs/mtcsexpmed.o aet/mtcs/mtcsexpr.o aet/mtcs/mtcsfinal.o\
    aet/mtcs/mtcsfunc.o  aet/mtcs/mtcsaddr.o\
    aet/mtcs/mtcslowersubreg.o aet/mtcs/mtcsmode.o aet/mtcs/mtcsopinit.o\
    aet/mtcs/mtcsoptabs.o aet/mtcs/mtcsoptions.o  aet/mtcs/mtcsoutput.o  aet/mtcs/mtcspassmgr.o\
    aet/mtcs/mtcspreds.o  aet/mtcs/mtcsreal.o aet/mtcs/mtcsrecog.o\
    aet/mtcs/mtcsreg.o aet/mtcs/mtcsreload.o aet/mtcs/mtcsrtl.o aet/mtcs/mtcsrtlanal.o\
    aet/mtcs/mtcsrtldata.o aet/mtcs/mtcssimplifyrtx.o aet/mtcs/mtcstarget.o aet/mtcs/mtcstool.o\
    aet/mtcs/mtcsvar.o aet/mtcs/mtcsvectorbuilder.o aet/mtcs/mtcscgraph.o\
    aet/mtcs/mtcsfuncabi.o aet/mtcs/mtcslibfuncs.o aet/mtcs/mtcspass.o aet/mtcs/mtcsconfig.o\
    aet/mtcs/mtcsopts.o aet/mtcs/mtcsclones.o \
    aet/mtcs/mtcsbuiltins.o aet/mtcs/mtcsstmt.o\
    aet/mtcs/mtcscfgrtl.o aet/mtcs/mtcstraversetree.o aet/mtcs/mtcstree.o aet/mtcs/mtcslang.o\
    aet/mtcs/mtcsstorlayout.o aet/mtcs/mtcsattribs.o aet/mtcs/mtcsprintrtl.o aet/mtcs/mtcscfgbuild.o\
    aet/mtcs/mtcscfgcleanup.o aet/mtcs/mtcscse.o aet/mtcs/mtcscfgstate.o aet/mtcs/mtcscfgcontext.o\
    aet/mtcs/mtcscfgrtlstate.o aet/mtcs/mtcscfglayoutstate.o aet/mtcs/mtcscfggimplestate.o\
    aet/mtcs/mtcscfg.o aet/mtcs/mtcsconst.o aet/mtcs/mtcsdfa.o aet/mtcs/mtcsfixed.o\
    aet/mtcs/mtcsunspec.o aet/mtcs/mtcsinsnattr.o aet/mtcs/mtcsdfscan.o \
    aet/mtcs/mtcsdfcore.o  aet/mtcs/mtcsdfproblems.o aet/mtcs/mtcsalias.o aet/mtcs/mtcsreload1.o\
    aet/mtcs/mtcsdce.o  aet/mtcs/mtcscselib.o aet/mtcs/mtcsssapropagate.o aet/mtcs/mtcsrange.o\
    aet/mtcs/mtcsrangecache.o aet/mtcs/mtcsrangestorage.o aet/mtcs/mtcsoutofssa.o aet/mtcs/mtcscfgloop.o\
    aet/mtcs/mtcsloopiv.o aet/mtcs/mtcscfgloopmanip.o aet/mtcs/mtcspredict.o aet/mtcs/mtcsinternalfn.o\
    aet/mtcs/mtcsport.o aet/mtcs/mtcsgimpleexpr.o aet/mtcs/mtcsssacoalesce.o  aet/mtcs/mtcsreplace.o\
    aet/mtcs/mtcsdwarf2codeview.o aet/mtcs/mtcsdebug.o aet/mtcs/mtcsdwarf2lineno.o aet/mtcs/mtcsdonothingdebug.o\
    aet/mtcs/mtcsgimple.o aet/mtcs/mtcsadjustpass.o aet/mtcs/mtcsssaaddress.o\
    \
    aet/mtcs/rtl/mtcsrtlpassmgr.o \
    aet/mtcs/rtl/mtcsfwprop.o \
    aet/mtcs/rtl/mtcscprop.o \
    aet/mtcs/rtl/mtcsgcse.o\
    aet/mtcs/rtl/mtcsifcvt.o \
    aet/mtcs/rtl/mtcsloopinit.o \
    \
   aet/mtcs/rtl/mtcsira.o\
   aet/mtcs/rtl/mtcsiraallocno.o\
   aet/mtcs/rtl/mtcsirabuild.o\
   aet/mtcs/rtl/mtcsiracolor.o\
   aet/mtcs/rtl/mtcsiracommon.o\
   aet/mtcs/rtl/mtcsiraconflicts.o\
   aet/mtcs/rtl/mtcsiracosts.o\
   aet/mtcs/rtl/mtcsiraemit.o\
   aet/mtcs/rtl/mtcsiraint.o\
   aet/mtcs/rtl/mtcsiralives.o\
   aet/mtcs/rtl/mtcsiralooptreenode.o\
   aet/mtcs/rtl/mtcsiraobject.o\
   aet/mtcs/rtl/mtcsweb.o\
   aet/mtcs/rtl/mtcsdse.o\
   aet/mtcs/rtl/mtcscombine.o\
   aet/mtcs/rtl/mtcsbbreorder.o\
   aet/mtcs/rtl/mtcspostreload.o\
   aet/mtcs/rtl/mtcsreorg.o\
   aet/mtcs/rtl/mtcsresource.o\
   aet/mtcs/rtl/mtcsloopinvariant.o\
   aet/mtcs/rtl/mtcsloopunroll.o\
   aet/mtcs/rtl/mtcsextdce.o\
   \
   aet/mtcs/rtl/ssa/mtcsaccesses.o\
   aet/mtcs/rtl/ssa/mtcsblocks.o\
   aet/mtcs/rtl/ssa/mtcschanges.o\
   aet/mtcs/rtl/ssa/mtcsfunctions.o\
   aet/mtcs/rtl/ssa/mtcsinsns.o\
   aet/mtcs/rtl/ssa/mtcsmovement.o\
   \
   aet/mtcs/machine/mtcsmachine.o \
   aet/mtcs/machine/targetmemtag.o\
   aet/mtcs/machine/targetc.o \
   aet/mtcs/machine/targetvectorize.o\
   aet/mtcs/machine/targetaddrspace.o\
   aet/mtcs/machine/targetoption.o\
   aet/mtcs/machine/targetcommon.o\
   aet/mtcs/machine/targetemutls.o\
   aet/mtcs/machine/targetasmout.o\
   aet/mtcs/machine/targetcalls.o\
   aet/mtcs/machine/targetrtx.o
   
   
AET_MTCS_PTX = aet/mtcs/ptx/mtcsptx.o aet/mtcs/ptx/mtcsptxalign.o aet/mtcs/ptx/mtcsptxargs.o aet/mtcs/ptx/mtcsptxcodes.o\
    aet/mtcs/ptx/mtcsptxemit.o aet/mtcs/ptx/mtcsptxfunc.o aet/mtcs/ptx/mtcsptxmode.o aet/mtcs/ptx/mtcsptxunspec.o\
    aet/mtcs/ptx/mtcsptxopinit.o aet/mtcs/ptx/mtcsptxoptions.o aet/mtcs/ptx/mtcsptxoutput.o aet/mtcs/ptx/mtcsptxpreds.o\
    aet/mtcs/ptx/mtcsptxreal.o aet/mtcs/ptx/mtcsptxrecog.o aet/mtcs/ptx/mtcsptxreg.o aet/mtcs/ptx/ptxtool.o\
    aet/mtcs/ptx/mtcsptxconfig.o  aet/mtcs/ptx/mtcsptxbuiltins.o aet/mtcs/ptx/mtcsptxtree.o aet/mtcs/ptx/mtcsptxattribs.o\
    aet/mtcs/ptx/mtcsptxinsnattr.o  aet/mtcs/ptx/mtcsptxrtl.o aet/mtcs/ptx/mtcsptxasm.o aet/mtcs/ptx/mtcsptxmath.o\
    aet/mtcs/ptx/mtcsptxinternalfn.o\
    aet/mtcs/ptx/targetptxvectorize.o aet/mtcs/ptx/targetptxaddrspace.o aet/mtcs/ptx/targetptxoption.o\
    aet/mtcs/ptx/targetptxcommon.o aet/mtcs/ptx/targetptxasmout.o aet/mtcs/ptx/targetptxcalls.o\
    aet/mtcs/ptx/targetptxrtx.o\
       \
    aet/mtcs/ptx/gen/ptx-insn-output.o\
    aet/mtcs/ptx/gen/ptx-insn-emit.o\
    aet/mtcs/ptx/gen/ptx-insn-recog.o\
    aet/mtcs/ptx/gen/ptx-insn-extract.o\
    aet/mtcs/ptx/gen/ptx-insn-unspec.o\
    aet/mtcs/ptx/gen/ptx-insn-attr.o\
    aet/mtcs/ptx/gen/ptx-insn-preds.o\
    aet/mtcs/ptx/gen/ptx-options.o\
    aet/mtcs/ptx/gen/ptx-insn-opinit.o\
    aet/mtcs/ptx/gen/ptx-insn-modes.o
    
#在源代码中的gen可能不存在，需要创建
GEN_DIR := $(srcdir)/aet/mtcs/ptx/gen
$(GEN_DIR):
	mkdir -p $@
$(GEN_DIR)/%.c: | $(GEN_DIR)
$(GEN_DIR)/%.h: | $(GEN_DIR)
	
AET_OBJS = $(AET_BASE) $(AET_TREE) $(AET_MTCS) $(AET_MTCS_PTX)

#------------------------编译mtcsgenxxx.c并用它们编译平台代码----------------
#build aet/natomic.o
build/natomic.o: aet/natomic.c 
#build aet/nmem.o
build/nmem.o: aet/nmem.c 
#build aet/nslice.o
build/nslice.o: aet/nslice.c 
#build aet/nlist.o
build/nlist.o: aet/nlist.c 
#build aet/nlog.o
build/nlog.o: aet/nlog.c 
#build aet/nqsort.o
build/nqsort.o: aet/nqsort.c 
#build aet/nptrarray.o
build/nptrarray.o: aet/nptrarray.c 
#build aet/nerror.o
build/nerror.o: aet/nerror.c 
#build aet/nprintf.o
build/nprintf.o: aet/nprintf.c 
#build aet/nstrfuncs.o
build/nstrfuncs.o: aet/nstrfuncs.c 
#build aet/nunicode.o
build/nunicode.o: aet/nunicode.c 
#build aet/nconvert.o
build/nconvert.o: aet/nconvert.c 
#build aet/nuniprop.o
build/nuniprop.o: aet/nuniprop.c 
#build aet/nstring.o
build/nstring.o: aet/nstring.c 
#build aet/nunidecomp.o
build/nunidecomp.o: aet/nunidecomp.c 
#build aet/nhash.o
build/nhash.o: aet/nhash.c 
 #build aet/nfileutils.o
build/nfileutils.o: aet/nfileutils.c 
#build aet/ncharset.o
build/ncharset.o: aet/ncharset.c 
#build aet/localcharset.o
build/localcharset.o: aet/localcharset.c 
 #build aet/nfile.o
build/nfile.o: aet/nfile.c 
 #build aet/mtcs/ptx/gen/ptx-insn-modes.c mtcsgen需要 ptx-insn-modes中的各种数据如modeName modeClass modeSize...
build/ptx-insn-modes.o: aet/mtcs/ptx/gen/ptx-insn-modes.c 
  
BUILD_NLIB =  build/natomic.o  build/nmem.o  build/nslice.o  build/nlist.o \
    build/nlog.o   build/nqsort.o build/nptrarray.o build/nerror.o \
    build/nprintf.o build/nstrfuncs.o  build/nunicode.o build/nconvert.o \
    build/nuniprop.o build/nstring.o build/nunidecomp.o build/nhash.o \
    build/nfileutils.o build/ncharset.o build/localcharset.o build/nfile.o  build/ptx-insn-modes.o
    
mtcs_modes_file=$(srcdir)/aet/mtcs/ptx/nvptx-modes.def -Wptx
#生成ptx-insn-modes.h头文件
ptx_modes_h=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-modes.h
$(ptx_modes_h): ptx-modes-h; @true
ptx-modes-h: $(MD_DEPS) build/mtcsgenmodes$(build_exeext)
	$(RUN_GEN) build/mtcsgenmodes$(build_exeext) $(mtcs_modes_file) -h > tmp-ptxmodes.h
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxmodes.h $(ptx_modes_h)
	$(STAMP) ptx-modes-h	
	
#生成ptx-insn-modes.c文件
ptx_modes_c=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-modes.c
$(ptx_modes_c): ptx-modes-c; @true
ptx-modes-c: $(MD_DEPS) build/mtcsgenmodes$(build_exeext)
	$(RUN_GEN) build/mtcsgenmodes$(build_exeext) $(mtcs_modes_file) -c > tmp-ptxmodes.c
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxmodes.c $(ptx_modes_c)
	$(STAMP) ptx-modes-c	
	

#$(AET_MTCS_PTX): $(ptx_modes_h) $(srcdir)/aet/mtcs/ptx/gen/ptx-insn-preds.h $(srcdir)/aet/mtcs/ptx/gen/ptx-insn-flags.h\
#$(srcdir)/aet/mtcs/ptx/gen/ptx-optionsitem.h $(srcdir)/aet/mtcs/ptx/gen/ptx-insn-attr.h $(srcdir)/aet/mtcs/ptx/gen/ptx-insn-unspec.h
#checking how to run the C preprocessor... make[2]: *** 没有规则可制作目标“../../gcc-151/gcc/aet/mtcs/ptx/gen/ptx-insn-preds.h”，
#由“aet/mtcs/ptx/mtcsptx.o” 需求。 停止。
#把ptx-insn-preds.h 从依赖中移走，问题解决
$(AET_MTCS_PTX): $(ptx_modes_h)  $(srcdir)/aet/mtcs/ptx/gen/ptx-insn-flags.h $(srcdir)/aet/mtcs/mtcsoptionsitem.h \
	$(srcdir)/aet/mtcs/ptx/gen/ptx-optionsitem.h $(srcdir)/aet/mtcs/ptx/gen/ptx-insn-attr.h $(srcdir)/aet/mtcs/ptx/gen/ptx-insn-unspec.h \
	$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-codes.h $(srcdir)/aet/mtcs/ptx/gen/ptx-insn-targetdef.h

aet/mtcs/ptx/gen/ptx-insn-emit.o aet/mtcs/ptx/mtcsptx.o: $(srcdir)/aet/mtcs/ptx/gen/ptx-insn-codes.h


#build mtcsgen.c mtcsgen.c依赖 xxx-insn-modes.h
build/mtcsgen.o: aet/mtcs/tool/mtcsgen.c $(BCONFIG_H) $(SYSTEM_H) 		\
  $(CORETYPES_H) $(GTM_H) $(RTL_BASE_H) $(OBSTACK_H) errors.h		\
  $(HASHTAB_H) $(READ_MD_H) $(GENSUPPORT_H) $(HASH_TABLE_H) $(ptx_modes_h)
  
  
#BUILD_COMPILERFLAGS 定义在makefile.in中 用来编译不在gcc环境中的源文件
BUILD_COMPILERFLAGS+=-DCONFIG_NLIB_HOST
#-----运行代码生成工具，生成新的文件 ptx-insn-output.c ptx-insn-recog.c ptx-insn-extract.c ptx-insn-emit.c ptx-options.c mtcsoptionsitem.h--
insn_conditons_md_file=$(srcdir)/aet/mtcs/ptx/insn-conditions.md

#生成文件的保存路径，由平台名+gen组成完整路径
save_file_root_path=$(srcdir)/aet/mtcs

ptx_md_file=$(srcdir)/common.md $(srcdir)/aet/mtcs/ptx/mtcs_ptx.md -Wptx

#新增mtcsgenflags.c 用来生成 xxx-insn-flags.h 依赖 $(AET_MTCS_PTX)
ptx_flags_header_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-flags.h
$(ptx_flags_header_dest): ptx-s-flags-h; @true
ptx-s-flags-h: $(MD_DEPS) build/mtcsgenflags$(build_exeext)
	$(RUN_GEN) build/mtcsgenflags$(build_exeext) $(ptx_md_file)  > tmp-ptxflags.h
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxflags.h $(ptx_flags_header_dest)
	$(STAMP) ptx-s-flags-h	

#新增mtcsgencodes.c 用来生成 xxx-insn-codes.h
ptx_codes_header_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-codes.h
$(ptx_codes_header_dest): ptx-s-codes-h; @true
ptx-s-codes-h: $(MD_DEPS) build/mtcsgencodes$(build_exeext)
	$(RUN_GEN) build/mtcsgencodes$(build_exeext) $(ptx_md_file) > tmp-ptxcodes.h
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxcodes.h $(ptx_codes_header_dest)
	$(STAMP) ptx-s-codes-h	

ptx_emit_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-emit.c
$(ptx_emit_dest): ptx-s-emit; @true
ptx-s-emit: $(MD_DEPS) build/mtcsgenemit$(build_exeext) $(MD_DEPS) $(insn_conditons_md_file)
	$(RUN_GEN) build/mtcsgenemit$(build_exeext) $(ptx_md_file) $(insn_conditons_md_file)  > tmp-ptx-emit.c
	$(SHELL) $(srcdir)/../move-if-change tmp-ptx-emit.c $(ptx_emit_dest)
	$(STAMP) ptx-s-emit
	
ptx_output_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-output.c
$(ptx_output_dest): ptx-s-output; @true
ptx-s-output: $(MD_DEPS) build/mtcsgenoutput$(build_exeext)
	$(RUN_GEN) build/mtcsgenoutput$(build_exeext) $(ptx_md_file) $(insn_conditons_md_file) > tmp-ptx-output.c
	$(SHELL) $(srcdir)/../move-if-change tmp-ptx-output.c $(ptx_output_dest)
	$(STAMP) ptx-s-output	
	
ptx_recog_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-recog.c
$(ptx_recog_dest): ptx-s-recog; @true
ptx-s-recog: $(MD_DEPS) build/mtcsgenrecog$(build_exeext)
	$(RUN_GEN) build/mtcsgenrecog$(build_exeext) $(ptx_md_file) $(insn_conditons_md_file) -S$(save_file_root_path) > tmp-ptx-recog.c
	$(SHELL) $(srcdir)/../move-if-change tmp-ptx-recog.c $(ptx_recog_dest)
	$(STAMP) ptx-s-recog	
	
ptx_extract_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-extract.c
$(ptx_extract_dest): ptx-s-extract; @true
ptx-s-extract: $(MD_DEPS) build/mtcsgenextract$(build_exeext)
	$(RUN_GEN) build/mtcsgenextract$(build_exeext) $(ptx_md_file) $(insn_conditons_md_file) > tmp-ptx-extract.c
	$(SHELL) $(srcdir)/../move-if-change tmp-ptx-extract.c $(ptx_extract_dest)
	$(STAMP) ptx-s-extract	
	
#生成ptx-insn-unspec.h头文件
ptx_unspec_header_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-unspec.h
$(ptx_unspec_header_dest): ptx-s-unspec-h; @true
ptx-s-unspec-h: $(MD_DEPS) build/mtcsgenconstants$(build_exeext)
	$(RUN_GEN) build/mtcsgenconstants$(build_exeext) $(ptx_md_file) -h > tmp-ptxunspec.h
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxunspec.h $(ptx_unspec_header_dest)
	$(STAMP) ptx-s-unspec-h	
	
#生成ptx-insn-unspec.c文件
ptx_unspec_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-unspec.c
$(ptx_unspec_dest): ptx-s-unspec; @true
ptx-s-unspec: $(MD_DEPS) build/mtcsgenconstants$(build_exeext)
	$(RUN_GEN) build/mtcsgenconstants$(build_exeext) $(ptx_md_file) -c > tmp-ptxunspec.c
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxunspec.c $(ptx_unspec_dest)
	$(STAMP) ptx-s-unspec	
	
#生成ptx-insn-attr.h头文件
ptx_attr_header_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-attr.h
$(ptx_attr_header_dest): ptx-s-attr-h; @true
ptx-s-attr-h: $(MD_DEPS) build/mtcsgenattr$(build_exeext)
	$(RUN_GEN) build/mtcsgenattr$(build_exeext) $(ptx_md_file)  > tmp-ptxattr.h
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxattr.h $(ptx_attr_header_dest)
	$(STAMP) ptx-s-attr-h	

# mtcsgenattrtab produces three files: tmp-{attrtab.cc,dfatab.cc,latencytab.cc}
#生成ptx-insn-attr.c文件

ptx_attr_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-attr.c
ptx_dfatab_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-dfatab.c
ptx_latencytab_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-latencytab.c
$(ptx_attr_dest) $(ptx_dfatab_dest) $(ptx_latencytab_dest): ptx-s-attrtab ; @true
ptx-s-attrtab: $(MD_DEPS) build/mtcsgenattrtab$(build_exeext)
	$(RUN_GEN) build/mtcsgenattrtab$(build_exeext) $(ptx_md_file) -Atmp-ptxattr.c -Dtmp-ptxdfatab.c -Ltmp-ptxlatencytab.c
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxattr.c $(ptx_attr_dest)
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxdfatab.c $(ptx_dfatab_dest)
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxlatencytab.c $(ptx_latencytab_dest)
	$(STAMP) ptx-s-attrtab

#编译mtcsgenconstants.c文件，是为了生成ptx-insn-constants.h,但该文件没有目标依赖，所以不会编译mtcsgenconstants.c
#加入需要依赖ptx-insn-constants.h的文件 ptx-insn-extract问题解决。

#生成mtcsoptionsitem.h 需要主机的options.h和设备的options.h  不能绑死绝对路径上
#options_argv=/home/sns/workspace/gcc153/src/build-host-gcc/gcc/options.h \
#            $(srcdir)/aet/mtcs/ptx/ptx_options.h 
    
#gcc/options.h 是主要的options.h 在build-host-gcc/gcc目录下，
#ptx_options.h 原名是 options.h 在build-nvptx-gcc/gcc目录下，拷到 aet 目录并改名为 ptx_options.h  
#解决bug 056

#有一个问题
#如果编译的gcc指定目标是nvptx-none --target=nvptx-none 生成的gcc/options.h内容
#与/home/sns/workspace/gcc153/src/gcc-153/gcc/aet/mtcs/ptx/ptx_options.h
#是一样了，生成的ptx-optionsitem.h内容中有
#typedef struct _PtxOptionsItem
#{
#MtcsOptionsItem parent;
#}PtxOptionsItem;
#无
#typedef struct _PtxOptionsItem
#{
#MtcsOptionsItem parent;
#int x_nvptx_alias;
#int x_nvptx_experimental;
#int x_fake_exceptions;
#int x_nvptx_fake_ptx_alloca;
#int x_nvptx_init_regs;
#int x_ptx_isa_option;
#int x_nvptx_optimize;
#int x_nvptx_comment;
#int x_ptx_version_option;
#int x_nvptx_softstack_size;
#int x_VAR_mmainkernel;
#
#}PtxOptionsItem;
#编译通不过。所以必须用主机的options.h

ifeq ($(target),nvptx-unknown-none)
options_argv=/home/sns/workspace/gcc153/src/build-host-gcc/gcc/options.h $(srcdir)/aet/mtcs/ptx/ptx_options.h 
else
options_argv :=$(shell pwd)/options.h $(srcdir)/aet/mtcs/ptx/ptx_options.h 
endif

options_argv+=-Wptx
 
mtcs_options_item_dest=$(srcdir)/aet/mtcs/mtcsoptionsitem.h
$(mtcs_options_item_dest): ptx-s-mtcsoptionsitem-h; @true

ptx_options_item_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-optionsitem.h
$(ptx_options_item_dest): ptx-s-optionsitem-h; @true
	
ptx_options_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-options.c
$(ptx_options_dest): ptx-s-options; @true

#-c告诉mtcsgenoptions生成 c文件 -h生成头文件
ptx-s-mtcsoptionsitem-h: build/mtcsgenoptions$(build_exeext)
	$(RUN_GEN) build/mtcsgenoptions$(build_exeext) $(options_argv) -h > tmp-mtcsoptionsitem.h
	$(SHELL) $(srcdir)/../move-if-change tmp-mtcsoptionsitem.h $(mtcs_options_item_dest)
	$(STAMP) ptx-s-mtcsoptionsitem-h
	
ptx-s-optionsitem-h: build/mtcsgenoptions$(build_exeext)
	$(RUN_GEN) build/mtcsgenoptions$(build_exeext) $(options_argv) -q > tmp-ptxoptionsitem.h
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxoptionsitem.h $(ptx_options_item_dest)
	$(STAMP) ptx-s-optionsitem-h
	
ptx-s-options: $(MD_DEPS) build/mtcsgenoptions$(build_exeext) $(MD_DEPS) 
	$(RUN_GEN) build/mtcsgenoptions$(build_exeext) $(options_argv) -c > tmp-ptx-options.c
	$(SHELL) $(srcdir)/../move-if-change tmp-ptx-options.c $(ptx_options_dest)
	$(STAMP) ptx-s-options
	
#新增mtcsgenpreds.c 在内部生成三个文件 xxx-insn-preds.h xxx-insn-constrs.h xxx-insn-preds.cc
ptx_preds_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-preds.c
$(ptx_preds_dest): ptx-s-preds; @true
ptx-s-preds: $(MD_DEPS) build/mtcsgenpreds$(build_exeext) $(MD_DEPS) 
	$(RUN_GEN) build/mtcsgenpreds$(build_exeext) $(ptx_md_file) -S$(save_file_root_path) > tmp-ptx-preds.c
	$(SHELL) $(srcdir)/../move-if-change tmp-ptx-preds.c $(ptx_preds_dest)
	$(STAMP) ptx-s-preds
	
# 新增 mtcsgenopinit.c 生成两个文件 ptx-insn-opinit.h ptx-insn-opinit.c.
ptx_opinit_h =$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-opinit.h
ptx_opinit_c =$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-opinit.c

$(ptx_opinit_c) $(ptx_opinit_h): ptx-s-opinit ; @true;
ptx-s-opinit: $(MD_DEPS) build/mtcsgenopinit$(build_exeext) $(MD_DEPS) $(insn_conditons_md_file)
	$(RUN_GEN) build/mtcsgenopinit$(build_exeext) $(ptx_md_file) $(insn_conditons_md_file) \
	 -htmp-opinit.h -ctmp-opinit.c
	$(SHELL) $(srcdir)/../move-if-change tmp-opinit.h $(ptx_opinit_h)
	$(SHELL) $(srcdir)/../move-if-change tmp-opinit.c $(ptx_opinit_c)
	$(STAMP) ptx-s-opinit

#新增mtcsgentargetdef.c 用来生成 xxx-insn-targetdef.h 依赖 $(AET_MTCS_PTX)
ptx_targetdef_header_dest=$(srcdir)/aet/mtcs/ptx/gen/ptx-insn-targetdef.h
$(ptx_targetdef_header_dest): ptx-s-targetdef-h; @true
ptx-s-targetdef-h: $(MD_DEPS) build/mtcsgentargetdef$(build_exeext)
	$(RUN_GEN) build/mtcsgentargetdef$(build_exeext) $(ptx_md_file)  > tmp-ptxtargetdef.h
	$(SHELL) $(srcdir)/../move-if-change tmp-ptxtargetdef.h $(ptx_targetdef_header_dest)
	$(STAMP) ptx-s-targetdef-h
		
#---------------完成 .c .h 文件生成-------------------- 

#--------编译13个代码生成工具------
#1.
build/mtcsgenemit.o : aet/mtcs/tool/mtcsgenemit.c $(RTL_BASE_H) $(BCONFIG_H) $(SYSTEM_H)	\
  $(CORETYPES_H) $(GTM_H) errors.h $(READ_MD_H) $(GENSUPPORT_H) internal-fn.def
#2.
build/mtcsgenoutput.o : aet/mtcs/tool/mtcsgenoutput.c $(RTL_BASE_H) $(BCONFIG_H) $(SYSTEM_H)	\
  $(CORETYPES_H) $(GTM_H) errors.h $(READ_MD_H) $(GENSUPPORT_H)
#3.  
build/mtcsgenrecog.o : aet/mtcs/tool/mtcsgenrecog.c $(RTL_BASE_H) $(BCONFIG_H) $(SYSTEM_H)	\
  $(CORETYPES_H) $(GTM_H) errors.h $(READ_MD_H) $(GENSUPPORT_H)		\
  $(HASH_TABLE_H) inchash.h 
#4. 
build/mtcsgenoptions.o : aet/mtcs/tool/mtcsgenoptions.c $(RTL_BASE_H) $(BCONFIG_H) $(SYSTEM_H)	\
  $(CORETYPES_H) $(GTM_H) errors.h $(READ_MD_H) $(GENSUPPORT_H)		\
  $(HASH_TABLE_H) inchash.h 
#5.
build/mtcsgenconstants.o : aet/mtcs/tool/mtcsgenconstants.c $(RTL_BASE_H) $(BCONFIG_H) $(SYSTEM_H)	\
  $(CORETYPES_H) $(GTM_H) errors.h $(READ_MD_H) $(GENSUPPORT_H) 
#6.
build/mtcsgenextract.o : aet/mtcs/tool/mtcsgenextract.c $(RTL_BASE_H) $(BCONFIG_H) $(SYSTEM_H) \
  $(CORETYPES_H) $(GTM_H) errors.h $(READ_MD_H) $(GENSUPPORT_H) 
#7.
build/mtcsgenattr.o : aet/mtcs/tool/mtcsgenattr.c $(RTL_BASE_H) $(BCONFIG_H) $(SYSTEM_H) \
  $(CORETYPES_H) $(GTM_H) errors.h $(READ_MD_H) $(GENSUPPORT_H) 
#8.
build/mtcsgenattrtab.o : aet/mtcs/tool/mtcsgenattrtab.c $(RTL_BASE_H) $(OBSTACK_H)		\
  $(BCONFIG_H) $(SYSTEM_H) $(CORETYPES_H) $(GTM_H) errors.h $(GGC_H)	\
  $(READ_MD_H) $(GENSUPPORT_H) $(FNMATCH_H)
#9. 
build/mtcsgenpreds.o : aet/mtcs/tool/mtcsgenpreds.c $(RTL_BASE_H) $(OBSTACK_H)		\
  $(BCONFIG_H) $(SYSTEM_H) $(CORETYPES_H) $(GTM_H) errors.h $(GGC_H)	\
  $(READ_MD_H) $(GENSUPPORT_H) $(FNMATCH_H)
#10. 
build/mtcsgenflags.o : aet/mtcs/tool/mtcsgenflags.c $(RTL_BASE_H) $(OBSTACK_H)		\
  $(BCONFIG_H) $(SYSTEM_H) $(CORETYPES_H) $(GTM_H) errors.h $(GGC_H)	\
  $(READ_MD_H) $(GENSUPPORT_H) $(FNMATCH_H)
#11.
build/mtcsgencodes.o : aet/mtcs/tool/mtcsgencodes.c $(RTL_BASE_H) $(OBSTACK_H)		\
  $(BCONFIG_H) $(SYSTEM_H) $(CORETYPES_H) $(GTM_H) errors.h $(GGC_H)	\
  $(READ_MD_H) $(GENSUPPORT_H) $(FNMATCH_H)
#12.
build/mtcsgenopinit.o : aet/mtcs/tool/mtcsgenopinit.c $(RTL_BASE_H) $(OBSTACK_H)		\
  $(BCONFIG_H) $(SYSTEM_H) $(CORETYPES_H) $(GTM_H) errors.h $(GGC_H)	\
  $(READ_MD_H) $(GENSUPPORT_H) $(FNMATCH_H)
#13.
build/mtcsgentargetdef.o : aet/mtcs/tool/mtcsgentargetdef.c $(RTL_BASE_H) $(OBSTACK_H)		\
  $(BCONFIG_H) $(SYSTEM_H) $(CORETYPES_H) $(GTM_H) errors.h $(GGC_H)	\
  $(READ_MD_H) $(GENSUPPORT_H) $(FNMATCH_H)
   
 
npvtx_modes_file=$(srcdir)/aet/mtcs/ptx/nvptx-modes.def
build/mtcsgenmodes.o : aet/mtcs/tool/mtcsgenmodes.c $(BCONFIG_H) $(SYSTEM_H) errors.h		\
  $(HASHTAB_H) machmode.def $(npvtx_modes_file)
  
# All these programs use the RTL reader ($(BUILD_RTL)).
#13个 mtcsgexxx 文件，不包括mtcsgenmodes.c
mgenprogrtl = output emit recog options extract constants attr attrtab preds flags codes opinit targetdef
$(mgenprogrtl:%=build/mtcsgen%$(build_exeext)): $(BUILD_RTL) $(BUILD_NLIB) 

# All these programs use the MD reader ($(BUILD_MD)).
mgenprogmd = $(mgenprogrtl) 
$(mgenprogmd:%=build/mtcsgen%$(build_exeext)): $(BUILD_MD)

# All these programs need to report errors.
mgenprogerr = $(mgenprogmd) 
$(mgenprogerr:%=build/mtcsgen%$(build_exeext)): $(BUILD_ERRORS)

# Remaining build programs.
mgenprog = $(mgenprogerr) 

#modes_file = /home/sns/workspace/gcc-151/src/build-host-gcc/gcc/aet/mtcs/ptx/gen/ptx-insn-modes.o
# Rule for the generator programs:
$(mgenprog:%=build/mtcsgen%$(build_exeext)): build/mtcsgen%$(build_exeext): build/mtcsgen%.o build/mtcsgen.o $(BUILD_LIBDEPS)
	+$(LINKER_FOR_BUILD) $(BUILD_LINKERFLAGS) $(BUILD_LDFLAGS) -o $@ \
	    $(filter-out $(BUILD_LIBDEPS), $^) $(BUILD_LIBS)
	    
#单独生成mtcsgenmodes，因为mtcsgenmodes 不依赖 mtcsgen
# All these programs use the RTL reader ($(BUILD_RTL)).
mgenprogrtl_1 = modes
$(mgenprogrtl_1:%=build/mtcsgen%$(build_exeext)): $(BUILD_RTL) 

# All these programs use the MD reader ($(BUILD_MD)).
mgenprogmd_1 = $(mgenprogrtl_1) 
$(mgenprogmd_1:%=build/mtcsgen%$(build_exeext)): $(BUILD_MD)

# All these programs need to report errors.
mgenprogerr_1 = $(mgenprogmd_1) 
$(mgenprogerr_1:%=build/mtcsgen%$(build_exeext)): $(BUILD_ERRORS)

# Remaining build programs.
mgenprog_1 = $(mgenprogerr_1) 

# Rule for the generator programs:
$(mgenprog_1:%=build/mtcsgen%$(build_exeext)): build/mtcsgen%$(build_exeext): build/mtcsgen%.o $(BUILD_LIBDEPS)
	+$(LINKER_FOR_BUILD) $(BUILD_LINKERFLAGS) $(BUILD_LDFLAGS) -o $@ \
	    $(filter-out $(BUILD_LIBDEPS), $^) $(BUILD_LIBS) 
	    

######################以下是libaet库的编译和安装#############################
# AET库的源文件
AET_LIB_SRC_DIR := $(srcdir)/aet/libaet
AET_LIB_INCLUDE_DIR := $(srcdir)/aet/libaet

# 递归查找所有 .c 文件
#AET_LIB_C_SOURCES := $(shell find $(AET_LIB_SRC_DIR) -name "*.c")
# 递归查找所有 .c 文件但排除cuda目录下的所有.c文件
AET_LIB_C_SOURCES := $(shell find $(AET_LIB_SRC_DIR) -name "*.c" -not -path "*/cuda/*")

# 转换为对象文件（在 build 目录中保持相同结构）
AET_LIB_OBJ_DIR := build/aet
AET_LIB_OBJECTS := $(patsubst $(AET_LIB_SRC_DIR)/%.c,$(AET_LIB_OBJ_DIR)/%.o,$(AET_LIB_C_SOURCES))

# 单独处理 cuda 文件
CUDA_SRC_DIR := $(AET_LIB_SRC_DIR)/aet/mtcs/cuda
CUDA_C_SOURCES := $(shell find $(CUDA_SRC_DIR) -name "*.c")
CUDA_OBJ_DIR := $(AET_LIB_OBJ_DIR)/aet/mtcs/cuda
CUDA_OBJECTS := $(patsubst $(CUDA_SRC_DIR)/%.c,$(CUDA_OBJ_DIR)/%.o,$(CUDA_C_SOURCES))

AET_MAIN_LIB_TARGET = build/libaet.so
AET_CUDA_LIB_TARGET = build/libaet_cuda.so

AET_LIB_TARGET := $(AET_MAIN_LIB_TARGET)
AET_LIB_HEADER_SRC_DIR := $(srcdir)/aet/libaet
#头文件的安装目录
AET_LIB_HEADER_INSTALL_DIR = $(DESTDIR)$(includedir)/libaet
# 递归查找头文件
AET_LIB_HEADERS = $(shell find $(AET_LIB_HEADER_SRC_DIR) -name "*.h")

AET_INSTALL_TARGET :=install-libaet install-aet-headers install-libdevice
# CUDA 自动检测
# 优先级: 1. 环境变量 2. which nvcc 3. 标准路径

# 首先检查 CUDA_HOME 环境变量
ifneq (,$(CUDA_HOME))
    CUDA_PATH := $(CUDA_HOME)
else ifneq (,$(CUDA_PATH))
    CUDA_PATH := $(CUDA_PATH)
else
    # 自动检测
    CUDA_PATH := $(shell \
        if command -v nvcc >/dev/null 2>&1; then \
            dirname $$(dirname $$(command -v nvcc)); \
        elif [ -f "/usr/local/cuda/bin/nvcc" ]; then \
            echo "/usr/local/cuda"; \
        elif [ -f "/opt/cuda/bin/nvcc" ]; then \
            echo "/opt/cuda"; \
        else \
            echo ""; \
        fi \
    )
endif

# 验证 CUDA 安装
ifneq (,$(CUDA_PATH))
    ifeq (,$(wildcard $(CUDA_PATH)/bin/nvcc))
        $(warning CUDA detected at $(CUDA_PATH) but nvcc not found)
        WITH_CUDA := 0
    else
        WITH_CUDA := 1
    endif
else
    WITH_CUDA := 0
endif

#WITH_CUDA := 0

# 输出检测结果
ifeq ($(WITH_CUDA),1)
    $(info CUDA detected: $(CUDA_PATH))
    
    # 获取 CUDA 版本
    CUDA_VERSION := $(shell $(CUDA_PATH)/bin/nvcc --version 2>/dev/null | \
        grep "release" | \
        sed 's/.*release //' | \
        sed 's/,.*//')
    
    $(info CUDA version: $(CUDA_VERSION))
    
    # 设置编译选项
    CUDA_INCLUDE := -I$(CUDA_PATH)/include
    CUDA_LIB := -L$(CUDA_PATH)/lib64 -lcudart
    #目标原来只有libaet.so 现在加入 libaet_cuda.so
    AET_LIB_TARGET +=$(AET_CUDA_LIB_TARGET)
    AET_INSTALL_TARGET +=install-libaetcuda
    # 检查是否有 nvcc
    NVCC := $(CUDA_PATH)/bin/nvcc
else
    $(info CUDA not detected)
    CUDA_INCLUDE :=
    CUDA_LIB :=
    NVCC :=
endif

HOST_TARGET=x86_64-pc-linux-gnu
#如果正在编译的目标是 HOST_TARGET 追加新目标 AET_LIB_TARGET
ifneq (,$(findstring $(HOST_TARGET),$(target)))
AET_ALL_OBJECTS_STAGE := $(AET_LIB_OBJECTS)
ifeq ($(WITH_CUDA),1)
  AET_ALL_OBJECTS_STAGE += $(CUDA_OBJECTS)
endif
maybe-libaet: $(AET_ALL_OBJECTS_STAGE)
all: maybe-libaet
install: $(AET_INSTALL_TARGET)
endif

AET_XGCC = ./xgcc -B./
LIBGCC_BUILD_DIR := $(objdir)/../$(target)/libgcc

# 如果用 $(CFLAGS) 系统默认的是 -g -o2 当编译MtcsTest.c时出错
AET_CFLAGS :=-O3
#编译aet中的库时一定要加该选项，否则报找不到libaet.so错，该选项只针对aet和测试程序，对用户程序不需要该选项，
AET_NOINCLUDE :=-noaetinclude

#解决找不到stddef.h stdlib.h等问题。
AET_INTERNAL_INCLUDES = \
	-I./include \
	-I$(srcdir)/ginclude \
	-I$(srcdir)/include

# 编译规则：保持目录结构 加入依赖 xgcc$(exeext) cc1$(exeext) stmp-fixinc
$(AET_LIB_OBJ_DIR)/%.o: $(AET_LIB_SRC_DIR)/%.c xgcc$(exeext) cc1$(exeext) stmp-int-hdrs stmp-fixinc 
	@echo "CFLAGS=$(CFLAGS)"
	@mkdir -p $(@D)  # 创建必要的目录
	$(AET_XGCC) $(AET_CFLAGS) $(AET_NOINCLUDE) -I$(AET_LIB_INCLUDE_DIR) $(AET_INTERNAL_INCLUDES) -fPIC -c -o $@ $<

ifeq ($(WITH_CUDA),1)
# 编译CUDA库
$(CUDA_OBJ_DIR)/%.o: $(CUDA_SRC_DIR)/%.c xgcc$(exeext) cc1$(exeext) stmp-int-hdrs stmp-fixinc 
	@echo "CFLAGS=$(CFLAGS)"
	@echo "CUDA_INCLUDE=$(CUDA_INCLUDE)"
	@echo "CUDA_LIB=$(CUDA_LIB)"
	@mkdir -p $(@D)  # 创建必要的目录
	$(AET_XGCC) $(AET_CFLAGS) $(AET_NOINCLUDE) -I$(AET_LIB_INCLUDE_DIR) $(AET_INTERNAL_INCLUDES) $(CUDA_INCLUDE) -fPIC -c -o $@ $<
endif

# 动态库 libaet.so
$(AET_MAIN_LIB_TARGET): $(AET_LIB_OBJECTS)
	$(AET_XGCC) $(AET_NOINCLUDE) -fPIC -shared  -o $@ $^ $(LDFLAGS) 
ifeq ($(WITH_CUDA),1)
# CUDA 动态库
$(AET_CUDA_LIB_TARGET): $(CUDA_OBJECTS)
	$(AET_XGCC) $(AET_NOINCLUDE) -fPIC -shared  -o $@ $^ $(LDFLAGS) $(CUDA_LIB)
endif
#安装libaet.so
install-libaet: $(AET_MAIN_LIB_TARGET)
	@echo 'INSTALL_PROGRAM=$(INSTALL_PROGRAM)'
	@echo 'libdir=$(libdir)'
	@echo 'bindir=$(bindir)'
	@echo 'DESTDIR=$(DESTDIR)'
	@echo 'libsubdir=$(libsubdir)'
	@echo 'libexecsubdir=$(libexecsubdir)'
	@echo 'includedir=$(includedir)'
	@echo 'target=$(target)'
	@echo "实际命令: cp build/libaet.so $(DESTDIR)$(libdir)64/libaet.so "
	@mkdir -p $(DESTDIR)$(libdir)64/ 
	@cp $(AET_MAIN_LIB_TARGET) $(DESTDIR)$(libdir)64/libaet.so
	
install-libaetcuda: $(AET_CUDA_LIB_TARGET)
	@echo 'INSTALL_PROGRAM=$(INSTALL_PROGRAM)'
	@echo 'libdir=$(libdir)'
	@echo 'bindir=$(bindir)'
	@echo 'DESTDIR=$(DESTDIR)'
	@echo 'libsubdir=$(libsubdir)'
	@echo 'libexecsubdir=$(libexecsubdir)'
	@echo 'includedir=$(includedir)'
	@echo 'target=$(target)'
	@echo "实际命令: cp build/libaet_cuda.so $(DESTDIR)$(libdir)64/libaet_cuda.so "
	@cp $(AET_CUDA_LIB_TARGET) $(DESTDIR)$(libdir)64/libaet_cuda.so
	
#安装头文件
install-aet-headers: installdirs
	@echo "安装 AET 头文件到 $(AET_LIB_HEADER_INSTALL_DIR)..."
	@if [ -n "$(AET_LIB_HEADERS)" ]; then \
	    $(mkinstalldirs) $(AET_LIB_HEADER_INSTALL_DIR); \
	    for header in $(AET_LIB_HEADERS); do \
	        rel_path=$$(echo $$header | sed 's|^$(AET_LIB_HEADER_SRC_DIR)/||'); \
	        install_dir=$(AET_LIB_HEADER_INSTALL_DIR)/$$(dirname $$rel_path); \
	        $(mkinstalldirs) $$install_dir; \
	        $(INSTALL_DATA) $$header $$install_dir/; \
	        echo "  $$header -> $$install_dir/$$(basename $$header)"; \
	    done; \
	    echo "安装完成: 共安装 $(words $(AET_LIB_HEADERS)) 个头文件"; \
	else \
	    echo "警告: 未找到 AET 头文件"; \
	fi
	
#安装平台的数学库文件
LIBDEVICE_PTX_SRC_DIR=$(srcdir)/aet/mtcs/ptx/
LIBDEVICE_PTX_FILES = $(shell find $(LIBDEVICE_PTX_SRC_DIR) -name "cuda_libdevice_*.ptx" -type f)
LIBDEVICE_PTX_TARGET_DIR = $(DESTDIR)$(libdir)64/

install-libdevice: installdirs
	@echo "开始安装libdevice文件..."
	@for file in $(LIBDEVICE_PTX_FILES); do \
		rel_path=$${file#$(LIBDEVICE_PTX_SRC_DIR)/}; \
		target_path=$(LIBDEVICE_PTX_TARGET_DIR); \
		$(INSTALL) -m 644 "$$file" "$$target_path"; \
		echo "安装11: $$rel_path -> $$target_path"; \
	done
	@echo "安装完成"
	
# 调试信息
$(info AET 源文件: $(AET_LIB_C_SOURCES))
$(info AET 对象文件: $(AET_LIB_OBJECTS))
