$!
$!  CRYPTO-LIB.COM
$!  Written By:  Robert Byer
$!               Vice-President
$!               A-Com Computing, Inc.
$!               byer@mail.all-net.net
$!
$!  Changes by Richard Levitte <richard@levitte.org>
$!
$!  This command files compiles and creates the "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB" 
$!  library for OpenSSL.  The "xxx" denotes the machine architecture of AXP
$!  or VAX.
$!
$!  It was re-written so it would try to determine what "C" compiler to use 
$!  or you can specify which "C" compiler to use.
$!
$!  Specify the following as P1 to build just that part or ALL to just
$!  build everything.
$!
$!    		LIBRARY    To just compile the [.xxx.EXE.CRYPTO]LIBCRYPTO.OLB Library.
$!    		APPS       To just compile the [.xxx.EXE.CRYPTO]*.EXE
$!		ALL	   To do both LIBRARY and APPS
$!
$!  Specify RSAREF as P2 to compile with the RSAREF library instead of
$!  the regular one.  If you specify NORSAREF it will compile with the
$!  regular RSAREF routines.  (Note: If you are in the United States
$!  you MUST compile with RSAREF unless you have a license from RSA).
$!
$!  Note: The RSAREF libraries are NOT INCLUDED and you have to
$!        download it from "ftp://ftp.rsa.com/rsaref".  You have to
$!        get the ".tar-Z" file as the ".zip" file dosen't have the
$!        directory structure stored.  You have to extract the file
$!        into the [.RSAREF] directory under the root directory as that
$!        is where the scripts will look for the files.
$!
$!  Specify DEBUG or NODEBUG as P3 to compile with or without debugger
$!  information.
$!
$!  Specify which compiler at P4 to try to compile under.
$!
$!	   VAXC	 For VAX C.
$!	   DECC	 For DEC C.
$!	   GNUC	 For GNU C.
$!
$!  If you don't speficy a compiler, it will try to determine which
$!  "C" compiler to use.
$!
$!  P5, if defined, sets a TCP/IP library to use, through one of the following
$!  keywords:
$!
$!	UCX		for UCX
$!	SOCKETSHR	for SOCKETSHR+NETLIB
$!
$!  P6, if defined, sets a compiler thread NOT needed on OpenVMS 7.1 (and up)
$!
$!  P7, if defined, sets a choice of crypto methods to compile.
$!  WARNING: this should only be done to recompile some part of an already
$!  fully compiled library.
$!
$!
$! Define A TCP/IP Library That We Will Need To Link To.
$! (That Is, If We Need To Link To One.)
$!
$ TCPIP_LIB = ""
$!
$! Check Which Architecture We Are Using.
$!
$ IF (F$GETSYI("CPU").GE.128)
$ THEN
$!
$!  The Architecture Is AXP
$!
$   ARCH := AXP
$!
$! Else...
$!
$ ELSE
$!
$!  The Architecture Is VAX.
$!
$   ARCH := VAX
$!
$! End The Architecture Check.
$!
$ ENDIF
$!
$! Define The Different Encryption Types.
$!
$ ENCRYPT_TYPES = "Basic,MD2,MD5,SHA,MDC2,HMAC,RIPEMD,"+ -
		  "DES,RC2,RC4,RC5,IDEA,BF,CAST,"+ -
		  "BN,RSA,DSA,DH,"+ -
		  "BUFFER,BIO,STACK,LHASH,RAND,ERR,OBJECTS,"+ -
		  "EVP,EVP_2,ASN1,ASN1_2,PEM,X509,X509V3,"+ -
		  "CONF,TXT_DB,PKCS7,PKCS12,COMP"
$ ENCRYPT_PROGRAMS = "DES,PKCS7"
$!
$! Check To Make Sure We Have Valid Command Line Parameters.
$!
$ GOSUB CHECK_OPTIONS
$!
$! Initialise logical names and such
$!
$ GOSUB INITIALISE
$!
$! Tell The User What Kind of Machine We Run On.
$!
$ WRITE SYS$OUTPUT "Compiling On A ",ARCH," Machine."
$!
$! Define The OBJ Directory.
$!
$ OBJ_DIR := SYS$DISK:[-.'ARCH'.OBJ.CRYPTO]
$!
$! Check To See If The Architecture Specific OBJ Directory Exists.
$!
$ IF (F$PARSE(OBJ_DIR).EQS."")
$ THEN
$!
$!  It Dosen't Exist, So Create It.
$!
$   CREATE/DIR 'OBJ_DIR'
$!
$! End The Architecture Specific OBJ Directory Check.
$!
$ ENDIF
$!
$! Define The EXE Directory.
$!
$ EXE_DIR := SYS$DISK:[-.'ARCH'.EXE.CRYPTO]
$!
$! Check To See If The Architecture Specific Directory Exists.
$!
$ IF (F$PARSE(EXE_DIR).EQS."")
$ THEN
$!
$!  It Dosen't Exist, So Create It.
$!
$   CREATE/DIRECTORY 'EXE_DIR'
$!
$! End The Architecture Specific Directory Check.
$!
$ ENDIF
$!
$! Define The Library Name.
$!
$ LIB_NAME := 'EXE_DIR'LIBCRYPTO.OLB
$!
$! Define The CRYPTO-LIB We Are To Use.
$!
$ CRYPTO_LIB := 'EXE_DIR'LIBCRYPTO.OLB
$!
$! Define The RSAREF-LIB We Are To Use.
$!
$ RSAREF_LIB := SYS$DISK:[-.'ARCH'.EXE.RSAREF]LIBRSAGLUE.OLB
$!
$! Check To See If We Already Have A "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB" Library...
$!
$ IF (F$SEARCH(LIB_NAME).EQS."")
$ THEN
$!
$! Guess Not, Create The Library.
$!
$   LIBRARY/CREATE/OBJECT 'LIB_NAME'
$!
$! End The Library Check.
$!
$ ENDIF
$!
$! Build our options file for the application
$!
$ GOSUB CHECK_OPT_FILE
$!
$! Define The Different Encryption "library" Strings.
$!
$ APPS_DES = "DES/DES,CBC3_ENC"
$ APPS_PKCS7 = "ENC/ENC;DEC/DEC;SIGN/SIGN;VERIFY/VERIFY,EXAMPLE"
$
$ LIB_ = "cryptlib,mem,mem_dbg,cversion,ex_data,tmdiff,cpt_err"
$ LIB_MD2 = "md2_dgst,md2_one"
$ LIB_MD5 = "md5_dgst,md5_one"
$ LIB_SHA = "sha_dgst,sha1dgst,sha_one,sha1_one"
$ LIB_MDC2 = "mdc2dgst,mdc2_one"
$ LIB_HMAC = "hmac"
$ LIB_RIPEMD = "rmd_dgst,rmd_one"
$ LIB_DES = "set_key,ecb_enc,cbc_enc,"+ -
	"ecb3_enc,cfb64enc,cfb64ede,cfb_enc,ofb64ede,"+ -
	"enc_read,enc_writ,ofb64enc,"+ -
	"ofb_enc,str2key,pcbc_enc,qud_cksm,rand_key,"+ -
	"des_enc,fcrypt_b,read2pwd,"+ -
	"fcrypt,xcbc_enc,read_pwd,rpc_enc,cbc_cksm,ede_cbcm_enc"
$ LIB_RC2 = "rc2_ecb,rc2_skey,rc2_cbc,rc2cfb64,rc2ofb64"
$ LIB_RC4 = "rc4_skey,rc4_enc"
$ LIB_RC5 = "rc5_skey,rc5_ecb,rc5_enc,rc5cfb64,rc5ofb64"
$ LIB_IDEA = "i_cbc,i_cfb64,i_ofb64,i_ecb,i_skey"
$ LIB_BF = "bf_skey,bf_ecb,bf_enc,bf_cfb64,bf_ofb64"
$ LIB_CAST = "c_skey,c_ecb,c_enc,c_cfb64,c_ofb64"
$ LIB_BN_ASM = "[.asm]vms.mar,vms-helper"
$ IF F$TRNLNM("OPENSSL_NO_ASM").OR.ARCH.EQS."AXP" THEN LIB_BN_ASM = "bn_asm"
$ LIB_BN = "bn_add,bn_div,bn_exp,bn_lib,bn_ctx,bn_mul,"+ -
	"bn_print,bn_rand,bn_shift,bn_word,bn_blind,"+ -
	"bn_gcd,bn_prime,bn_err,bn_sqr,"+LIB_BN_ASM+",bn_recp,bn_mont,"+ -
	"bn_mpi,bn_exp2"
$ LIB_RSA = "rsa_eay,rsa_gen,rsa_lib,rsa_sign,rsa_saos,rsa_err,"+ -
	"rsa_pk1,rsa_ssl,rsa_none,rsa_oaep,rsa_chk,rsa_null"
$ LIB_DSA = "dsa_gen,dsa_key,dsa_lib,dsa_asn1,dsa_vrf,dsa_sign,dsa_err,dsa_ossl"
$ LIB_DH = "dh_gen,dh_key,dh_lib,dh_check,dh_err"
$ LIB_BUFFER = "buffer,buf_err"
$ LIB_BIO = "bio_lib,bio_cb,bio_err,"+ -
	"bss_mem,bss_null,bss_fd,"+ -
	"bss_file,bss_sock,bss_conn,"+ -
	"bf_null,bf_buff,b_print,b_dump,"+ -
	"b_sock,bss_acpt,bf_nbio,bss_rtcp,bss_bio,bss_log"
$ LIB_STACK = "stack"
$ LIB_LHASH = "lhash,lh_stats"
$ LIB_RAND = "md_rand,randfile,rand_lib,rand_err,rand_egd"
$ LIB_ERR = "err,err_all,err_prn"
$ LIB_OBJECTS = "o_names,obj_dat,obj_lib,obj_err"
$ LIB_EVP = "encode,digest,evp_enc,evp_key,"+ -
	"e_ecb_d,e_cbc_d,e_cfb_d,e_ofb_d,"+ -
	"e_ecb_i,e_cbc_i,e_cfb_i,e_ofb_i,"+ -
	"e_ecb_3d,e_cbc_3d,e_rc4,names,"+ -
	"e_cfb_3d,e_ofb_3d,e_xcbc_d,"+ -
	"e_ecb_r2,e_cbc_r2,e_cfb_r2,e_ofb_r2,"+ -
	"e_ecb_bf,e_cbc_bf,e_cfb_bf,e_ofb_bf"
$ LIB_EVP_2 = "e_ecb_c,e_cbc_c,e_cfb_c,e_ofb_c,"+ -
	"e_ecb_r5,e_cbc_r5,e_cfb_r5,e_ofb_r5,"+ -
	"m_null,m_md2,m_md5,m_sha,m_sha1,m_dss,m_dss1,m_mdc2,"+ -
	"m_ripemd,"+ -
	"p_open,p_seal,p_sign,p_verify,p_lib,p_enc,p_dec,"+ -
	"bio_md,bio_b64,bio_enc,evp_err,e_null,"+ -
	"c_all,c_allc,c_alld,evp_lib,bio_ok,"+-
	"evp_pkey,evp_pbe,p5_crpt,p5_crpt2"
$ LIB_ASN1 = "a_object,a_bitstr,a_utctm,a_gentm,a_time,a_int,a_octet,"+ -
	"a_null,a_print,a_type,a_set,a_dup,a_d2i_fp,a_i2d_fp,a_bmp,"+ -
	"a_enum,a_vis,a_utf8,a_sign,a_digest,a_verify,a_mbstr,"+ -
	"x_algor,x_val,x_pubkey,x_sig,x_req,x_attrib,"+ -
	"x_name,x_cinf,x_x509,x_x509a,x_crl,x_info,x_spki,nsseq,"+ -
	"d2i_r_pr,i2d_r_pr,d2i_r_pu,i2d_r_pu,"+ -
	"d2i_s_pr,i2d_s_pr,d2i_s_pu,i2d_s_pu,"+ -
	"d2i_pu,d2i_pr,i2d_pu,i2d_pr"
$ LIB_ASN1_2 = "t_req,t_x509,t_x509a,t_crl,t_pkey,t_spki,t_bitst,"+ -
	"p7_i_s,p7_signi,p7_signd,p7_recip,p7_enc_c,p7_evp,"+ -
	"p7_dgst,p7_s_e,p7_enc,p7_lib,"+ -
	"f_int,f_string,i2d_dhp,i2d_dsap,d2i_dhp,d2i_dsap,n_pkey,"+ -
	"f_enum,a_hdr,x_pkey,a_bool,x_exten,"+ -
	"asn1_par,asn1_lib,asn1_err,a_meth,a_bytes,a_strnid,"+ -
	"evp_asn1,asn_pack,p5_pbe,p5_pbev2,p8_pkey"
$ LIB_PEM = "pem_sign,pem_seal,pem_info,pem_lib,pem_all,pem_err"
$ LIB_X509 = "x509_def,x509_d2,x509_r2x,x509_cmp,"+ -
	"x509_obj,x509_req,x509spki,x509_vfy,"+ -
	"x509_set,x509rset,x509_err,"+ -
	"x509name,x509_v3,x509_ext,x509_att,"+ -
	"x509type,x509_lu,x_all,x509_txt,"+ -
	"x509_trs,by_file,by_dir"
$ LIB_X509V3 = "v3_bcons,v3_bitst,v3_conf,v3_extku,v3_ia5,v3_lib,"+ -
	"v3_prn,v3_utl,v3err,v3_genn,v3_alt,v3_skey,v3_akey,v3_pku,"+ -
	"v3_int,v3_enum,v3_sxnet,v3_cpols,v3_crld,v3_purp,v3_info"
$ LIB_CONF = "conf,conf_err"
$ LIB_TXT_DB = "txt_db"
$ LIB_PKCS7 = "pk7_lib,pkcs7err,pk7_doit,pk7_smime,pk7_attr,pk7_mime"
$ LIB_PKCS12 = "p12_add,p12_attr,p12_bags,p12_crpt,p12_crt,p12_decr,"+ -
	"p12_init,p12_key,p12_kiss,p12_lib,p12_mac,p12_mutl,"+ -
	"p12_sbag,p12_utl,p12_npas,pk12err"
$ LIB_COMP = "comp_lib,"+ -
	"c_rle,c_zlib"
$!
$! Setup exceptional compilations
$!
$ COMPILEWITH_CC3 = ",bss_rtcp,"
$ COMPILEWITH_CC4 = ",a_utctm,bss_log,"
$ COMPILEWITH_CC5 = ",md2_dgst,md5_dgst,mdc2dgst,sha_dgst,sha1dgst," + -
                    "rmd_dgst,bf_enc,"
$!
$! Check To See If We Are Going To Use RSAREF.
$!
$ IF (RSAREF.EQS."TRUE" .AND. ENCRYPT_TYPES - "RSA".NES.ENCRYPT_TYPES -
      .AND. (BUILDALL .EQS. "TRUE" .OR. BUILDALL .EQS. "LIBRARY"))
$ THEN
$!
$!  Check To See If The File [-.RSAREF]RSAREF.C Is Actually There.
$!
$   IF (F$SEARCH("SYS$DISK:[-.RSAREF]RSAREF.C").EQS."")
$   THEN
$!
$!    Tell The User That The File Dosen't Exist.
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The File [-.RSAREF]RSAREF.C Dosen't Exist."
$     WRITE SYS$OUTPUT ""
$!
$!    Exit The Build.
$!
$     GOTO EXIT
$!
$!  End The [-.RSAREF]RSAREF.C Check.
$!
$   ENDIF
$!
$!  Tell The User We Are Compiling The [-.RSAREF]RSAREF File.
$!
$   WRITE SYS$OUTPUT "Compiling The [-.RSAREF]RSAREF File."
$!
$!  Compile [-.RSAREF]RSAREF.C
$!
$   CC/OBJECT='OBJ_DIR'RSAREF.OBJ SYS$DISK:[-.RSAREF]RSAREF.C
$!
$!  Add It To The Library.
$!
$   LIBRARY/REPLACE 'LIB_NAME' 'OBJ_DIR'RSAREF.OBJ
$!
$!  Delete The Object File.
$!
$   DELETE 'OBJ_DIR'RSAREF.OBJ;*
$!
$!  Check To See If The File [-.RSAREF]RSAR_ERR.C Is Actually There.
$!
$   IF (F$SEARCH("SYS$DISK:[-.RSAREF]RSAR_ERR.C").EQS."")
$   THEN
$!
$!    Tell The User That The File Dosen't Exist.
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The File [-.RSAREF]RSAR_ERR.C Dosen't Exist."
$     WRITE SYS$OUTPUT ""
$!
$!    Exit The Build.
$!
$     GOTO EXIT
$!
$!  End The [-.RSAREF]RSAR_ERR.C File Check.
$!
$   ENDIF
$!
$!  Tell The User We Are Compiling The [-.RSAREF]RSAR_ERR File.
$!
$   WRITE SYS$OUTPUT "Compiling The [-.RSAREF]RSAR_ERR File."
$!
$!  Compile [-.RSAREF]RSAR_ERR.C
$!
$   CC/OBJECT='OBJ_DIR'RSAR_ERR.OBJ SYS$DISK:[-.RSAREF]RSAR_ERR.C
$!
$!  Add It To The Library.
$!
$   LIBRARY/REPLACE 'LIB_NAME' 'OBJ_DIR'RSAR_ERR.OBJ
$!
$!  Delete The Object File.
$!
$   DELETE 'OBJ_DIR'RSAR_ERR.OBJ;*
$!
$! End The RSAREF Check.
$!
$ ENDIF
$!
$! Figure Out What Other Modules We Are To Build.
$!
$ BUILD_SET:
$!
$! Define A Module Counter.
$!
$ MODULE_COUNTER = 0
$!
$! Top Of The Loop.
$!
$ MODULE_NEXT:
$!
$! Extract The Module Name From The Encryption List.
$!
$ MODULE_NAME = F$ELEMENT(MODULE_COUNTER,",",ENCRYPT_TYPES)
$ IF MODULE_NAME.EQS."Basic" THEN MODULE_NAME = ""
$ MODULE_NAME1 = MODULE_NAME
$!
$! Check To See If We Are At The End Of The Module List.
$!
$ IF (MODULE_NAME.EQS.",") 
$ THEN 
$!
$!  We Are At The End Of The Module List, Go To MODULE_DONE.
$!
$   GOTO MODULE_DONE
$!
$! End The Module List Check.
$!
$ ENDIF
$!
$! Increment The Moudle Counter.
$!
$ MODULE_COUNTER = MODULE_COUNTER + 1
$!
$! Create The Library and Apps Module Names.
$!
$ LIB_MODULE = "LIB_" + MODULE_NAME
$ APPS_MODULE = "APPS_" + MODULE_NAME
$ IF (MODULE_NAME.EQS."ASN1_2")
$ THEN
$   MODULE_NAME = "ASN1"
$ ENDIF
$ IF (MODULE_NAME.EQS."EVP_2")
$ THEN
$   MODULE_NAME = "EVP"
$ ENDIF
$!
$! Set state (can be LIB and APPS)
$!
$ STATE = "LIB"
$ IF BUILDALL .EQS. "APPS" THEN STATE = "APPS"
$!
$! Check if the library module name actually is defined
$!
$ IF F$TYPE('LIB_MODULE') .EQS. ""
$ THEN
$   WRITE SYS$ERROR ""
$   WRITE SYS$ERROR "The module ",MODULE_NAME," does not exist.  Continuing..."
$   WRITE SYS$ERROR ""
$   GOTO MODULE_NEXT
$ ENDIF
$!
$! Top Of The Module Loop.
$!
$ MODULE_AGAIN:
$!
$! Tell The User What Module We Are Building.
$!
$ IF (MODULE_NAME1.NES."") 
$ THEN
$   IF STATE .EQS. "LIB"
$   THEN
$     WRITE SYS$OUTPUT "Compiling The ",MODULE_NAME1," Library Files. (",BUILDALL,",",STATE,")"
$   ELSE IF F$TYPE('APPS_MODULE') .NES. ""
$     THEN
$       WRITE SYS$OUTPUT "Compiling The ",MODULE_NAME1," Applications. (",BUILDALL,",",STATE,")"
$     ENDIF
$   ENDIF
$ ENDIF
$!
$!  Define A File Counter And Set It To "0".
$!
$ FILE_COUNTER = 0
$ APPLICATION = ""
$ APPLICATION_COUNTER = 0
$!
$! Top Of The File Loop.
$!
$ NEXT_FILE:
$!
$! Look in the LIB_MODULE is we're in state LIB
$!
$ IF STATE .EQS. "LIB"
$ THEN
$!
$!   O.K, Extract The File Name From The File List.
$!
$   FILE_NAME = F$ELEMENT(FILE_COUNTER,",",'LIB_MODULE')
$!
$!   else
$!
$ ELSE
$   FILE_NAME = ","
$!
$   IF F$TYPE('APPS_MODULE') .NES. ""
$   THEN
$!
$!     Extract The File Name From The File List.
$!     This part is a bit more complicated.
$!
$     IF APPLICATION .EQS. ""
$     THEN
$       APPLICATION = F$ELEMENT(APPLICATION_COUNTER,";",'APPS_MODULE')
$       APPLICATION_COUNTER = APPLICATION_COUNTER + 1
$       APPLICATION_OBJECTS = F$ELEMENT(1,"/",APPLICATION)
$       APPLICATION = F$ELEMENT(0,"/",APPLICATION)
$       FILE_COUNTER = 0
$     ENDIF
$
$!     WRITE SYS$OUTPUT "DEBUG: SHOW SYMBOL APPLICATION*"
$!     SHOW SYMBOL APPLICATION*
$!
$     IF APPLICATION .NES. ";"
$     THEN
$       FILE_NAME = F$ELEMENT(FILE_COUNTER,",",APPLICATION_OBJECTS)
$       IF FILE_NAME .EQS. ","
$       THEN
$         APPLICATION = ""
$         GOTO NEXT_FILE
$       ENDIF
$     ENDIF
$   ENDIF
$ ENDIF
$!
$! Check To See If We Are At The End Of The File List.
$!
$ IF (FILE_NAME.EQS.",") 
$ THEN 
$!
$!  We Are At The End Of The File List, Change State Or Goto FILE_DONE.
$!
$   IF STATE .EQS. "LIB" .AND. BUILDALL .NES. "LIBRARY"
$   THEN
$     STATE = "APPS"
$     GOTO MODULE_AGAIN
$   ELSE
$     GOTO FILE_DONE
$   ENDIF
$!
$! End The File List Check.
$!
$ ENDIF
$!
$! Increment The Counter.
$!
$ FILE_COUNTER = FILE_COUNTER + 1
$!
$! Create The Source File Name.
$!
$ TMP_FILE_NAME = F$ELEMENT(1,"]",FILE_NAME)
$ IF TMP_FILE_NAME .EQS. "]" THEN TMP_FILE_NAME = FILE_NAME
$ IF F$ELEMENT(0,".",TMP_FILE_NAME) .EQS. TMP_FILE_NAME THEN -
	FILE_NAME = FILE_NAME + ".c"
$ IF (MODULE_NAME.NES."")
$ THEN
$   SOURCE_FILE = "SYS$DISK:[." + MODULE_NAME+ "]" + FILE_NAME
$ ELSE
$   SOURCE_FILE = "SYS$DISK:[]" + FILE_NAME
$ ENDIF
$ SOURCE_FILE = SOURCE_FILE - "]["
$!
$! Create The Object File Name.
$!
$ OBJECT_FILE = OBJ_DIR + F$PARSE(FILE_NAME,,,"NAME","SYNTAX_ONLY") + ".OBJ"
$ ON WARNING THEN GOTO NEXT_FILE
$!
$! Check To See If The File We Want To Compile Is Actually There.
$!
$ IF (F$SEARCH(SOURCE_FILE).EQS."")
$ THEN
$!
$!  Tell The User That The File Dosen't Exist.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The File ",SOURCE_FILE," Dosen't Exist."
$   WRITE SYS$OUTPUT ""
$!
$!  Exit The Build.
$!
$   GOTO EXIT
$!
$! End The File Exist Check.
$!
$ ENDIF
$!
$! Tell The User We Are Compiling The File.
$!
$ IF (MODULE_NAME.EQS."")
$ THEN
$   WRITE SYS$OUTPUT "Compiling The ",FILE_NAME," File.  (",BUILDALL,",",STATE,")"
$ ENDIF
$ IF (MODULE_NAME.NES."")
$ THEN 
$   WRITE SYS$OUTPUT "	",FILE_NAME,""
$ ENDIF
$!
$! Compile The File.
$!
$ ON ERROR THEN GOTO NEXT_FILE
$ FILE_NAME0 = F$ELEMENT(0,".",FILE_NAME)
$ IF FILE_NAME - ".mar" .NES. FILE_NAME
$ THEN
$   MACRO/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$ ELSE
$   IF COMPILEWITH_CC3 - FILE_NAME0 .NES. COMPILEWITH_CC3
$   THEN
$     CC3/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$   ELSE
$     IF COMPILEWITH_CC4 - FILE_NAME0 .NES. COMPILEWITH_CC4
$     THEN
$       CC4/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$     ELSE
$       IF COMPILEWITH_CC5 - FILE_NAME0 .NES. COMPILEWITH_CC5
$       THEN
$         CC5/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$       ELSE
$         CC/OBJECT='OBJECT_FILE' 'SOURCE_FILE'
$       ENDIF
$     ENDIF
$   ENDIF
$ ENDIF
$ IF STATE .EQS. "LIB"
$ THEN 
$!
$!   Add It To The Library.
$!
$   LIBRARY/REPLACE 'LIB_NAME' 'OBJECT_FILE'
$!
$!   Time To Clean Up The Object File.
$!
$   DELETE 'OBJECT_FILE';*
$ ENDIF
$!
$! Go Back And Do It Again.
$!
$ GOTO NEXT_FILE
$!
$! All Done With This Library Part.
$!
$ FILE_DONE:
$!
$! Time To Build Some Applications
$!
$ IF F$TYPE('APPS_MODULE') .NES. "" .AND. BUILDALL .NES. "LIBRARY"
$ THEN
$   APPLICATION_COUNTER = 0
$ NEXT_APPLICATION:
$   APPLICATION = F$ELEMENT(APPLICATION_COUNTER,";",'APPS_MODULE')
$   IF APPLICATION .EQS. ";" THEN GOTO APPLICATION_DONE
$
$   APPLICATION_COUNTER = APPLICATION_COUNTER + 1
$   APPLICATION_OBJECTS = F$ELEMENT(1,"/",APPLICATION)
$   APPLICATION = F$ELEMENT(0,"/",APPLICATION)
$
$!   WRITE SYS$OUTPUT "DEBUG: SHOW SYMBOL APPLICATION*"
$!   SHOW SYMBOL APPLICATION*
$!
$! Tell the user what happens
$!
$   WRITE SYS$OUTPUT "	",APPLICATION,".exe"
$!
$! Link The Program, Check To See If We Need To Link With RSAREF Or Not.
$!
$   IF (RSAREF.EQS."TRUE")
$   THEN
$!
$!  Check To See If We Are To Link With A Specific TCP/IP Library.
$!
$     IF (TCPIP_LIB.NES."")
$     THEN
$!
$!    Link With The RSAREF Library And A Specific TCP/IP Library.
$!
$       LINK/'DEBUGGER'/'TRACEBACK'/EXE='EXE_DIR''APPLICATION'.EXE -
            'OBJ_DIR''APPLICATION_OBJECTS', -
	    'CRYPTO_LIB'/LIBRARY,'RSAREF_LIB'/LIBRARY, -
	    'TCPIP_LIB','OPT_FILE'/OPTION
$!
$!    Else...
$!
$     ELSE
$!
$!      Link With The RSAREF Library And NO TCP/IP Library.
$!
$       LINK/'DEBUGGER'/'TRACEBACK'/EXE='EXE_DIR''APPLICATION'.EXE -
            'OBJ_DIR''APPLICATION_OBJECTS', -
	    'CRYPTO_LIB'/LIBRARY,'RSAREF_LIB'/LIBRARY, -
	    'OPT_FILE'/OPTION
$!
$!    End The TCP/IP Library Check.
$!
$     ENDIF
$!
$!   Else...
$!
$   ELSE
$!
$!    Don't Link With The RSAREF Routines.
$!
$!
$!    Check To See If We Are To Link With A Specific TCP/IP Library.
$!
$     IF (TCPIP_LIB.NES."")
$     THEN
$!
$!      Don't Link With The RSAREF Routines And TCP/IP Library.
$!
$       LINK/'DEBUGGER'/'TRACEBACK'/EXE='EXE_DIR''APPLICATION'.EXE -
            'OBJ_DIR''APPLICATION_OBJECTS', -
	    'CRYPTO_LIB'/LIBRARY, -
            'TCPIP_LIB','OPT_FILE'/OPTION
$!
$!    Else...
$!
$     ELSE
$!
$!      Don't Link With The RSAREF Routines And Link With A TCP/IP Library.
$!
$       LINK/'DEBUGGER'/'TRACEBACK'/EXE='EXE_DIR''APPLICATION'.EXE -
            'OBJ_DIR''APPLICATION_OBJECTS',-
	    'CRYPTO_LIB'/LIBRARY, -
            'OPT_FILE'/OPTION
$!
$!    End The TCP/IP Library Check.
$!
$     ENDIF
$!
$!   End The RSAREF Link Check.
$!
$   ENDIF
$   GOTO NEXT_APPLICATION
$  APPLICATION_DONE:
$ ENDIF
$!
$! Go Back And Get The Next Module.
$!
$ GOTO MODULE_NEXT
$!
$! All Done With This Module.
$!
$ MODULE_DONE:
$!
$! Tell The User That We Are All Done.
$!
$ WRITE SYS$OUTPUT "All Done..."
$ EXIT:
$ GOSUB CLEANUP
$ EXIT
$!
$! Check For The Link Option FIle.
$!
$ CHECK_OPT_FILE:
$!
$! Check To See If We Need To Make A VAX C Option File.
$!
$ IF (COMPILER.EQS."VAXC")
$ THEN
$!
$!  Check To See If We Already Have A VAX C Linker Option File.
$!
$   IF (F$SEARCH(OPT_FILE).EQS."")
$   THEN
$!
$!    We Need A VAX C Linker Option File.
$!
$     CREATE 'OPT_FILE'
$DECK
!
! Default System Options File To Link Agianst 
! The Sharable VAX C Runtime Library.
!
SYS$SHARE:VAXCRTL.EXE/SHARE
$EOD
$!
$!  End The Option File Check.
$!
$   ENDIF
$!
$! End The VAXC Check.
$!
$ ENDIF
$!
$! Check To See If We Need A GNU C Option File.
$!
$ IF (COMPILER.EQS."GNUC")
$ THEN
$!
$!  Check To See If We Already Have A GNU C Linker Option File.
$!
$   IF (F$SEARCH(OPT_FILE).EQS."")
$   THEN
$!
$!    We Need A GNU C Linker Option File.
$!
$     CREATE 'OPT_FILE'
$DECK
!
! Default System Options File To Link Agianst 
! The Sharable C Runtime Library.
!
GNU_CC:[000000]GCCLIB/LIBRARY
SYS$SHARE:VAXCRTL/SHARE
$EOD
$!
$!  End The Option File Check.
$!
$   ENDIF
$!
$! End The GNU C Check.
$!
$ ENDIF
$!
$! Check To See If We Need A DEC C Option File.
$!
$ IF (COMPILER.EQS."DECC")
$ THEN
$!
$!  Check To See If We Already Have A DEC C Linker Option File.
$!
$   IF (F$SEARCH(OPT_FILE).EQS."")
$   THEN
$!
$!    Figure Out If We Need An AXP Or A VAX Linker Option File.
$!
$     IF ARCH .EQS. "VAX"
$     THEN
$!
$!      We Need A DEC C Linker Option File For VAX.
$!
$       CREATE 'OPT_FILE'
$DECK
!
! Default System Options File To Link Agianst 
! The Sharable DEC C Runtime Library.
!
SYS$SHARE:DECC$SHR.EXE/SHARE
$EOD
$!
$!    Else...
$!
$     ELSE
$!
$!      Create The AXP Linker Option File.
$!
$       CREATE 'OPT_FILE'
$DECK
!
! Default System Options File For AXP To Link Agianst 
! The Sharable C Runtime Library.
!
SYS$SHARE:CMA$OPEN_LIB_SHR/SHARE
SYS$SHARE:CMA$OPEN_RTL/SHARE
$EOD
$!
$!    End The VAX/AXP DEC C Option File Check.
$!
$     ENDIF
$!
$!  End The Option File Search.
$!
$   ENDIF
$!
$! End The DEC C Check.
$!
$ ENDIF
$!
$!  Tell The User What Linker Option File We Are Using.
$!
$ WRITE SYS$OUTPUT "Using Linker Option File ",OPT_FILE,"."	
$!
$! Time To RETURN.
$!
$ RETURN
$!
$! Check The User's Options.
$!
$ CHECK_OPTIONS:
$!
$! Check To See If P1 Is Blank.
$!
$ IF (P1.EQS."ALL")
$ THEN
$!
$!   P1 Is Blank, So Build Everything.
$!
$    BUILDALL = "TRUE"
$!
$! Else...
$!
$ ELSE
$!
$!  Else, Check To See If P1 Has A Valid Arguement.
$!
$   IF (P1.EQS."LIBRARY").OR.(P1.EQS."APPS")
$   THEN
$!
$!    A Valid Arguement.
$!
$     BUILDALL = P1
$!
$!  Else...
$!
$   ELSE
$!
$!    Tell The User We Don't Know What They Want.
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P1," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "    ALL      :  Just Build Everything."
$     WRITE SYS$OUTPUT "    LIBRARY  :  To Compile Just The [.xxx.EXE.SSL]LIBCRYPTO.OLB Library."
$     WRITE SYS$OUTPUT "    APPS     :  To Compile Just The [.xxx.EXE.SSL]*.EXE Programs."
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT " Where 'xxx' Stands For:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "        AXP  :  Alpha Architecture."
$     WRITE SYS$OUTPUT "        VAX  :  VAX Architecture."
$     WRITE SYS$OUTPUT ""
$!
$!    Time To EXIT.
$!
$     EXIT
$!
$!  End The Valid Arguement Check.
$!
$   ENDIF
$!
$! End The P1 Check.
$!
$ ENDIF
$!
$! Check To See If P2 Is Blank.
$!
$ IF (P2.EQS."NORSAREF")
$ THEN
$!
$!   P2 Is NORSAREF, So Compile With The Regular RSA Libraries.
$!
$    RSAREF = "FALSE"
$ ELSE
$!
$!  Check To See If We Are To Use The RSAREF Library.
$!
$   IF (P2.EQS."RSAREF")
$   THEN
$!
$!    Check To Make Sure We Have The RSAREF Source Code Directory.
$!
$     IF (F$SEARCH("SYS$DISK:[-.RSAREF]SOURCE.DIR").EQS."")
$     THEN
$!
$!      We Don't Have The RSAREF Souce Code Directory, So Tell The
$!      User This.
$!
$       WRITE SYS$OUTPUT ""
$       WRITE SYS$OUTPUT "It appears that you don't have the RSAREF Souce Code."
$       WRITE SYS$OUTPUT "You need to go to 'ftp://ftp.rsa.com/rsaref'.  You have to"
$       WRITE SYS$OUTPUT "get the '.tar-Z' file as the '.zip' file dosen't have the"
$       WRITE SYS$OUTPUT "directory structure stored.  You have to extract the file"
$       WRITE SYS$OUTPUT "into the [.RSAREF] directory under the root directory"
$       WRITE SYS$OUTPUT "as that is where the scripts will look for the files."
$       WRITE SYS$OUTPUT ""
$!
$!      Time To Exit.
$!
$       EXIT
$!
$!    Else, Compile Using The RSAREF Library.
$!
$     ELSE
$       RSAREF = "TRUE"
$     ENDIF
$   ELSE 
$!
$!    They Entered An Invalid Option..
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P2," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "     RSAREF   :  Compile With The RSAREF Library."
$     WRITE SYS$OUTPUT "     NORSAREF :  Compile With The Regular RSA Library."
$     WRITE SYS$OUTPUT ""
$!
$!    Time To EXIT.
$!
$     EXIT
$!
$!  End The Valid Arguement Check.
$!
$   ENDIF
$!
$! End The P2 Check.
$!
$ ENDIF
$!
$! Check To See If P3 Is Blank.
$!
$ IF (P3.EQS."NODEBUG")
$ THEN
$!
$!   P3 Is NODEBUG, So Compile Without The Debugger Information.
$!
$    DEBUGGER = "NODEBUG"
$    TRACEBACK = "NOTRACEBACK" 
$    GCC_OPTIMIZE = "OPTIMIZE"
$    CC_OPTIMIZE = "OPTIMIZE"
$    MACRO_OPTIMIZE = "OPTIMIZE"
$    WRITE SYS$OUTPUT "No Debugger Information Will Be Produced During Compile."
$    WRITE SYS$OUTPUT "Compiling With Compiler Optimization."
$ ELSE
$!
$!  Check To See If We Are To Compile With Debugger Information.
$!
$   IF (P3.EQS."DEBUG")
$   THEN
$!
$!    Compile With Debugger Information.
$!
$     DEBUGGER = "DEBUG"
$     TRACEBACK = "TRACEBACK"
$     GCC_OPTIMIZE = "NOOPTIMIZE"
$     CC_OPTIMIZE = "NOOPTIMIZE"
$     MACRO_OPTIMIZE = "NOOPTIMIZE"
$     WRITE SYS$OUTPUT "Debugger Information Will Be Produced During Compile."
$     WRITE SYS$OUTPUT "Compiling Without Compiler Optimization."
$   ELSE 
$!
$!    They Entered An Invalid Option..
$!
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "The Option ",P3," Is Invalid.  The Valid Options Are:"
$     WRITE SYS$OUTPUT ""
$     WRITE SYS$OUTPUT "     DEBUG   :  Compile With The Debugger Information."
$     WRITE SYS$OUTPUT "     NODEBUG :  Compile Without The Debugger Information."
$     WRITE SYS$OUTPUT ""
$!
$!    Time To EXIT.
$!
$     EXIT
$!
$!  End The Valid Arguement Check.
$!
$   ENDIF
$!
$! End The P3 Check.
$!
$ ENDIF
$!
$! Special Threads For OpenVMS v7.1 Or Later
$!
$! Written By:  Richard Levitte
$!              richard@levitte.org
$!
$!
$! Check To See If We Have A Option For P6.
$!
$ IF (P6.EQS."")
$ THEN
$!
$!  Get The Version Of VMS We Are Using.
$!
$   ISSEVEN :=
$   TMP = F$ELEMENT(0,"-",F$EXTRACT(1,4,F$GETSYI("VERSION")))
$   TMP = F$INTEGER(F$ELEMENT(0,".",TMP)+F$ELEMENT(1,".",TMP))
$!
$!  Check To See If The VMS Version Is v7.1 Or Later.
$!
$   IF (TMP.GE.71)
$   THEN
$!
$!    We Have OpenVMS v7.1 Or Later, So Use The Special Threads.
$!
$     ISSEVEN := ,PTHREAD_USE_D4
$!
$!  End The VMS Version Check.
$!
$   ENDIF
$!
$! End The P6 Check.
$!
$ ENDIF
$!
$! Check To See If P4 Is Blank.
$!
$ IF (P4.EQS."")
$ THEN
$!
$!  O.K., The User Didn't Specify A Compiler, Let's Try To
$!  Find Out Which One To Use.
$!
$!  Check To See If We Have GNU C.
$!
$   IF (F$TRNLNM("GNU_CC").NES."")
$   THEN
$!
$!    Looks Like GNUC, Set To Use GNUC.
$!
$     P4 = "GNUC"
$!
$!  Else...
$!
$   ELSE
$!
$!    Check To See If We Have VAXC Or DECC.
$!
$     IF (ARCH.EQS."AXP").OR.(F$TRNLNM("DECC$CC_DEFAULT").NES."")
$     THEN 
$!
$!      Looks Like DECC, Set To Use DECC.
$!
$       P4 = "DECC"
$!
$!    Else...
$!
$     ELSE
$!
$!      Looks Like VAXC, Set To Use VAXC.
$!
$       P4 = "VAXC"
$!
$!    End The VAXC Compiler Check.
$!
$     ENDIF
$!
$!  End The DECC & VAXC Compiler Check.
$!
$   ENDIF
$!
$!  End The Compiler Check.
$!
$ ENDIF
$!
$! Check To See If We Have A Option For P5.
$!
$ IF (P5.EQS."")
$ THEN
$!
$!  Find out what socket library we have available
$!
$   IF F$PARSE("SOCKETSHR:") .NES. ""
$   THEN
$!
$!    We have SOCKETSHR, and it is my opinion that it's the best to use.
$!
$     P5 = "SOCKETSHR"
$!
$!    Tell the user
$!
$     WRITE SYS$OUTPUT "Using SOCKETSHR for TCP/IP"
$!
$!    Else, let's look for something else
$!
$   ELSE
$!
$!    Like UCX (the reason to do this before Multinet is that the UCX
$!    emulation is easier to use...)
$!
$     IF F$TRNLNM("UCX$IPC_SHR") .NES. "" -
	 .OR. F$PARSE("SYS$SHARE:UCX$IPC_SHR.EXE") .NES. "" -
	 .OR. F$PARSE("SYS$LIBRARY:UCX$IPC.OLB") .NES. ""
$     THEN
$!
$!	Last resort: a UCX or UCX-compatible library
$!
$	P5 = "UCX"
$!
$!      Tell the user
$!
$       WRITE SYS$OUTPUT "Using UCX or an emulation thereof for TCP/IP"
$!
$!	That was all...
$!
$     ENDIF
$   ENDIF
$ ENDIF
$!
$! Set Up Initial CC Definitions, Possibly With User Ones
$!
$ CCDEFS = "VMS=1,TCPIP_TYPE_''P5'"
$ IF F$TRNLNM("OPENSSL_NO_ASM") THEN CCDEFS = CCDEFS + ",NO_ASM"
$ IF F$TRNLNM("OPENSSL_NO_RSA") THEN CCDEFS = CCDEFS + ",NO_RSA"
$ IF F$TRNLNM("OPENSSL_NO_DSA") THEN CCDEFS = CCDEFS + ",NO_DSA"
$ IF F$TRNLNM("OPENSSL_NO_DH") THEN CCDEFS = CCDEFS + ",NO_DH"
$ IF F$TRNLNM("OPENSSL_NO_MD2") THEN CCDEFS = CCDEFS + ",NO_MD2"
$ IF F$TRNLNM("OPENSSL_NO_MD5") THEN CCDEFS = CCDEFS + ",NO_MD5"
$ IF F$TRNLNM("OPENSSL_NO_RIPEMD") THEN CCDEFS = CCDEFS + ",NO_RIPEMD"
$ IF F$TRNLNM("OPENSSL_NO_SHA") THEN CCDEFS = CCDEFS + ",NO_SHA"
$ IF F$TRNLNM("OPENSSL_NO_SHA0") THEN CCDEFS = CCDEFS + ",NO_SHA0"
$ IF F$TRNLNM("OPENSSL_NO_SHA1") THEN CCDEFS = CCDEFS + ",NO_SHA1"
$ IF F$TRNLNM("OPENSSL_NO_DES")
$ THEN
$   CCDEFS = CCDEFS + ",NO_DES,NO_MDC2"
$ ELSE
$   IF F$TRNLNM("OPENSSL_NO_MDC2") THEN CCDEFS = CCDEFS + ",NO_MDC2"
$ ENDIF
$ IF F$TRNLNM("OPENSSL_NO_RC2") THEN CCDEFS = CCDEFS + ",NO_RC2"
$ IF F$TRNLNM("OPENSSL_NO_RC4") THEN CCDEFS = CCDEFS + ",NO_RC4"
$ IF F$TRNLNM("OPENSSL_NO_RC5") THEN CCDEFS = CCDEFS + ",NO_RC5"
$ IF F$TRNLNM("OPENSSL_NO_IDEA") THEN CCDEFS = CCDEFS + ",NO_IDEA"
$ IF F$TRNLNM("OPENSSL_NO_BF") THEN CCDEFS = CCDEFS + ",NO_BF"
$ IF F$TRNLNM("OPENSSL_NO_CAST") THEN CCDEFS = CCDEFS + ",NO_CAST"
$ IF F$TRNLNM("OPENSSL_NO_HMAC") THEN CCDEFS = CCDEFS + ",NO_HMAC"
$ IF F$TRNLNM("OPENSSL_NO_SSL2") THEN CCDEFS = CCDEFS + ",NO_SSL2"
$ IF F$TYPE(USER_CCDEFS) .NES. "" THEN CCDEFS = CCDEFS + "," + USER_CCDEFS
$ CCEXTRAFLAGS = ""
$ IF F$TYPE(USER_CCFLAGS) .NES. "" THEN CCEXTRAFLAGS = USER_CCFLAGS
$ CCDISABLEWARNINGS = "LONGLONGTYPE,LONGLONGSUFX"
$ IF F$TYPE(USER_CCDISABLEWARNINGS) .NES. "" THEN -
	CCDISABLEWARNINGS = CCDISABLEWARNINGS + "," + USER_CCDISABLEWARNINGS
$!
$!  Check To See If The User Entered A Valid Paramter.
$!
$ IF (P4.EQS."VAXC").OR.(P4.EQS."DECC").OR.(P4.EQS."GNUC")
$ THEN
$!
$!    Check To See If The User Wanted DECC.
$!
$   IF (P4.EQS."DECC")
$   THEN
$!
$!    Looks Like DECC, Set To Use DECC.
$!
$     COMPILER = "DECC"
$!
$!    Tell The User We Are Using DECC.
$!
$     WRITE SYS$OUTPUT "Using DECC 'C' Compiler."
$!
$!    Use DECC...
$!
$     CC = "CC"
$     IF ARCH.EQS."VAX" .AND. F$TRNLNM("DECC$CC_DEFAULT").NES."/DECC" -
	 THEN CC = "CC/DECC"
$     CC = CC + "/''CC_OPTIMIZE'/''DEBUGGER'/STANDARD=ANSI89" + -
           "/NOLIST/PREFIX=ALL/INCLUDE=SYS$DISK:[]" + CCEXTRAFLAGS
$!
$!    Define The Linker Options File Name.
$!
$     OPT_FILE = "SYS$DISK:[]VAX_DECC_OPTIONS.OPT"
$!
$!  End DECC Check.
$!
$   ENDIF
$!
$!  Check To See If We Are To Use VAXC.
$!
$   IF (P4.EQS."VAXC")
$   THEN
$!
$!    Looks Like VAXC, Set To Use VAXC.
$!
$     COMPILER = "VAXC"
$!
$!    Tell The User We Are Using VAX C.
$!
$     WRITE SYS$OUTPUT "Using VAXC 'C' Compiler."
$!
$!    Compile Using VAXC.
$!
$     CC = "CC"
$     IF ARCH.EQS."AXP"
$     THEN
$	WRITE SYS$OUTPUT "There is no VAX C on Alpha!"
$	EXIT
$     ENDIF
$     IF F$TRNLNM("DECC$CC_DEFAULT").EQS."/DECC" THEN CC = "CC/VAXC"
$     CC = CC + "/''CC_OPTIMIZE'/''DEBUGGER'/NOLIST/INCLUDE=SYS$DISK:[]" + -
	   CCEXTRAFLAGS
$     CCDEFS = """VAXC""," + CCDEFS
$!
$!    Define <sys> As SYS$COMMON:[SYSLIB]
$!
$     DEFINE/NOLOG SYS SYS$COMMON:[SYSLIB]
$!
$!    Define The Linker Options File Name.
$!
$     OPT_FILE = "SYS$DISK:[]VAX_VAXC_OPTIONS.OPT"
$!
$!  End VAXC Check
$!
$   ENDIF
$!
$!  Check To See If We Are To Use GNU C.
$!
$   IF (P4.EQS."GNUC")
$   THEN
$!
$!    Looks Like GNUC, Set To Use GNUC.
$!
$     COMPILER = "GNUC"
$!
$!    Tell The User We Are Using GNUC.
$!
$     WRITE SYS$OUTPUT "Using GNU 'C' Compiler."
$!
$!    Use GNU C...
$!
$     CC = "GCC/NOCASE_HACK/''GCC_OPTIMIZE'/''DEBUGGER'/NOLIST" + -
	   "/INCLUDE=SYS$DISK:[]" + CCEXTRAFLAGS
$!
$!    Define The Linker Options File Name.
$!
$     OPT_FILE = "SYS$DISK:[]VAX_GNUC_OPTIONS.OPT"
$!
$!  End The GNU C Check.
$!
$   ENDIF
$!
$!  Set up default defines
$!
$   CCDEFS = """FLAT_INC=1""," + CCDEFS
$!
$!  Check To See If We Are To Compile With RSAREF Routines.
$!
$   IF (RSAREF.EQS."TRUE")
$   THEN
$!
$!    Compile With RSAREF.
$!
$     CCDEFS = CCDEFS + ",""RSAref=1"""
$!
$!    Tell The User This.
$!
$     WRITE SYS$OUTPUT "Compiling With RSAREF Routines."
$!
$!    Else, We Don't Care.  Compile Without The RSAREF Library.
$!
$   ELSE
$!
$!    Tell The User We Are Compile Without The RSAREF Routines.
$!
$     WRITE SYS$OUTPUT "Compiling Without The RSAREF Routines.
$!
$!  End The RSAREF Check.
$!
$   ENDIF
$!
$!  Finish up the definition of CC.
$!
$   IF COMPILER .EQS. "DECC"
$   THEN
$     IF CCDISABLEWARNINGS .EQS. ""
$     THEN
$       CC4DISABLEWARNINGS = "DOLLARID"
$     ELSE
$       CC4DISABLEWARNINGS = CCDISABLEWARNINGS + ",DOLLARID"
$       CCDISABLEWARNINGS = "/WARNING=(DISABLE=(" + CCDISABLEWARNINGS + "))"
$     ENDIF
$     CC4DISABLEWARNINGS = "/WARNING=(DISABLE=(" + CC4DISABLEWARNINGS + "))"
$   ELSE
$     CCDISABLEWARNINGS = ""
$     CC4DISABLEWARNINGS = ""
$   ENDIF
$   CC3 = CC + "/DEFINE=(" + CCDEFS + ISSEVEN + ")" + CCDISABLEWARNINGS
$   CC = CC + "/DEFINE=(" + CCDEFS + ")" + CCDISABLEWARNINGS
$   IF ARCH .EQS. "VAX" .AND. COMPILER .EQS. "DECC" .AND. P3 .NES. "DEBUG"
$   THEN
$     CC5 = CC + "/OPTIMIZE=NODISJOINT"
$   ELSE
$     CC5 = CC + "/NOOPTIMIZE"
$   ENDIF
$   CC4 = CC - CCDISABLEWARNINGS + CC4DISABLEWARNINGS
$!
$!  Show user the result
$!
$   WRITE SYS$OUTPUT "Main C Compiling Command: ",CC
$!
$!  Else The User Entered An Invalid Arguement.
$!
$ ELSE
$!
$!  Tell The User We Don't Know What They Want.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The Option ",P4," Is Invalid.  The Valid Options Are:"
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "    VAXC  :  To Compile With VAX C."
$   WRITE SYS$OUTPUT "    DECC  :  To Compile With DEC C."
$   WRITE SYS$OUTPUT "    GNUC  :  To Compile With GNU C."
$   WRITE SYS$OUTPUT ""
$!
$!  Time To EXIT.
$!
$   EXIT
$!
$! End The Valid Arguement Check.
$!
$ ENDIF
$!
$! Build a MACRO command for the architecture at hand
$!
$ IF ARCH .EQS. "VAX" THEN MACRO = "MACRO/''DEBUGGER'"
$ IF ARCH .EQS. "AXP" THEN MACRO = "MACRO/MIGRATION/''DEBUGGER'/''MACRO_OPTIMIZE'"
$!
$!  Show user the result
$!
$   WRITE SYS$OUTPUT "Main MACRO Compiling Command: ",MACRO
$!
$! Time to check the contents, and to make sure we get the correct library.
$!
$ IF P5.EQS."SOCKETSHR" .OR. P5.EQS."MULTINET" .OR. P5.EQS."UCX"
$ THEN
$!
$!  Check to see if SOCKETSHR was chosen
$!
$   IF P5.EQS."SOCKETSHR"
$   THEN
$!
$!    Set the library to use SOCKETSHR
$!
$     TCPIP_LIB = "[-.VMS]SOCKETSHR_SHR.OPT/OPT"
$!
$!    Done with SOCKETSHR
$!
$   ENDIF
$!
$!  Check to see if MULTINET was chosen
$!
$   IF P5.EQS."MULTINET"
$   THEN
$!
$!    Set the library to use UCX emulation.
$!
$     P5 = "UCX"
$!
$!    Done with MULTINET
$!
$   ENDIF
$!
$!  Check to see if UCX was chosen
$!
$   IF P5.EQS."UCX"
$   THEN
$!
$!    Set the library to use UCX.
$!
$     TCPIP_LIB = "[-.VMS]UCX_SHR_DECC.OPT/OPT"
$     IF F$TRNLNM("UCX$IPC_SHR") .NES. ""
$     THEN
$       TCPIP_LIB = "[-.VMS]UCX_SHR_DECC_LOG.OPT/OPT"
$     ELSE
$       IF COMPILER .NES. "DECC" .AND. ARCH .EQS. "VAX" THEN -
	  TCPIP_LIB = "[-.VMS]UCX_SHR_VAXC.OPT/OPT"
$     ENDIF
$!
$!    Done with UCX
$!
$   ENDIF
$!
$!  Print info
$!
$   WRITE SYS$OUTPUT "TCP/IP library spec: ", TCPIP_LIB
$!
$!  Else The User Entered An Invalid Arguement.
$!
$ ELSE
$!
$!  Tell The User We Don't Know What They Want.
$!
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "The Option ",P5," Is Invalid.  The Valid Options Are:"
$   WRITE SYS$OUTPUT ""
$   WRITE SYS$OUTPUT "    SOCKETSHR  :  To link with SOCKETSHR TCP/IP library."
$   WRITE SYS$OUTPUT "    UCX        :  To link with UCX TCP/IP library."
$   WRITE SYS$OUTPUT ""
$!
$!  Time To EXIT.
$!
$   EXIT
$!
$!  Done with TCP/IP libraries
$!
$ ENDIF
$!
$! Check if the user wanted to compile just a subset of all the encryption
$! methods.
$!
$ IF P7 .NES. ""
$ THEN
$   ENCRYPT_TYPES = P7
$! NYI:   ENCRYPT_PROGRAMS = P7
$ ENDIF
$!
$!  Time To RETURN...
$!
$ RETURN
$!
$ INITIALISE:
$!
$! Save old value of the logical name OPENSSL
$!
$ __SAVE_OPENSSL = F$TRNLNM("OPENSSL","LNM$PROCESS_TABLE")
$!
$! Save directory information
$!
$ __HERE = F$PARSE(F$PARSE("A.;",F$ENVIRONMENT("PROCEDURE"))-"A.;","[]A.;") - "A.;"
$ __TOP = __HERE - "CRYPTO]"
$ __INCLUDE = __TOP + "INCLUDE.OPENSSL]"
$!
$! Set up the logical name OPENSSL to point at the include directory
$!
$ DEFINE OPENSSL/NOLOG '__INCLUDE'
$!
$! Done
$!
$ RETURN
$!
$ CLEANUP:
$!
$! Restore the logical name OPENSSL if it had a value
$!
$ IF __SAVE_OPENSSL .EQS. ""
$ THEN
$   DEASSIGN OPENSSL
$ ELSE
$   DEFINE/NOLOG OPENSSL '__SAVE_OPENSSL'
$ ENDIF
$!
$! Done
$!
$ RETURN
