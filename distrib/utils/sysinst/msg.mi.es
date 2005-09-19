/*	$NetBSD: msg.mi.es,v 1.4.2.2 2005/09/19 21:10:50 tron Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* MI Message catalog -- spanish, machine independent */

message usage
{uso: sysinst [-r release] [-f fichero-definici�n]
}

message sysinst_message_language
{Mensajes de instalaci�n en Espa�ol}

message Yes {Si}
message No {No}
message All {Todo}
message Some {Algunos}
message None {Ninguno}
message none {ninguno}
message OK {OK}
message ok {ok}
message On {Encendido}
message Off {Apagado}
message unchanged {sin cambios}
message Delete {borrar?}

message install
{instalar}

message reinstall
{reinstalar sets para}

message upgrade
{actualizar}

message hello
{Bienvenido a sysinst, la herramienta de instalaci�n de NetBSD-@@VERSION@@.
Esta herramienta guiada por men�s, est� dise�ada para ayudarle a instalar
NetBSD en un disco duro, o actualizar un sistema NetBSD existente, con
un trabajo minimo.
En los siguientes men�s teclee la letra de referencia (a b, c, ...) para
seleccionar el item, o teclee CTRL+N/CTRL+P para seleccionar la opci�n
siguiente/anterior.
Las teclas de flechas y AvPag/RePag tambi�n funcionan.
Active la seleccion actual desde el men� pulsando la tecla enter.

}

message thanks
{�Gracias por usar NetBSD!

}

message installusure
{Ha escogido instalar NetBSD en su disco duro. Esto cambiar� informaci�n
de su disco duro. �Deber�a haber hecho una copia de seguridad completa
antes de este procedimiento! Este procedimiento realizar� las siguientes
operaciones :
	a) Particionar su disco
	b) Crear nuevos sistemas de archivos BSD
	c) Cargar e instalar los sets de distribuci�n
	d) Algunas configuraciones iniciales del sistema

(Despues de introducir la informaci�n de las particiones pero antes de que
su disco sea cambiado, tendr� la oportunidad de salir del programa.

�Deberiamos continuar?
}

message upgradeusure
{Esta bien, vamos a actualizar NetBSD en su disco duro. Sin embargo, esto
cambiar� informaci�n de su disco duro. �Deber�a hacer una copia de seguridad
completa antes de este procedimiento! �Realmente desea actualizar NetBSD?
(Este es su �ltimo aviso antes de que el programa empiece a modificar
sus discos.)
}

message reinstallusure
{Esta bien, vamos a desempaquetar los sets de la distribuci�n NetBSD
a un disco duro marcado como iniciable.
Este procedimiento solo baja y desempaqueta los sets en un disco iniciable
pre-particionado. No pone nombre a discos, actualiza bootblocks, o guarda
cualquier informaci�n de configuraci�n. (Salga y escoja 'instalar' o
'actualizar' si quiere esas opciones.) �Ya deber�a haber hecho un
'instalar' o 'actualizar' antes de iniciar este procedimiento!

�Realmente quiere reinstalar los sets de la distribuci�n NetBSD?
(Este es su �ltimo aviso antes de que el programa empiece a modificar
sus discos.)
}


message nodisk
{No puedo encontrar ningun disco duro para usar con NetBSD. Volver�
al menu original.
}

message onedisk
{Solamente he encontrado un disco, %s.
Por eso asumir� que quiere instalar NetBSD en %s.
}

message ask_disk
{�En que disco quiere instalar NetBSD? }

message Available_disks
{Discos disponibles}

message cylinders
{cilindros}

message heads
{cabezales}

message sectors
{sectores}

message fs_isize
{tama�o promedio del fichero (bytes)}

message mountpoint
{punto de montaje (o 'ninguno')}

message cylname
{cil}

message secname
{sec}

message megname
{MB}

message layout
{NetBSD usa BSD disklabel para particionar la porci�n NetBSD del disco
en multiples particiones BSD. Ahora deber�a configurar su BSD disklabel.

Puede usar un editor simple para ajustar los tama�os de las particiones NetBSD,
o dejar las particiones existentes, tama�os y contenidos.

Entonces tendr� la oportunidad de cambiar cualquier campo de disklabel.

La parte NetBSD de su disco es de %d Megabytes.
Una instalaci�n completa requiere al menos %d Megabytes sin X y
al menos %d Megabytes si los sets de X son incluidos.
}

message Choose_your_size_specifier
{Seleccionando megabytes dar� tama�os de particiones cercanas a su
selecci�n, pero alineados a los limites de los cilindros.
Seleccionando sectores le permitir� especificar los tama�os de manera
mas precisa. En discos ZBR modernos, el tama�o actual del cilindro var�a
durante el disco y hay una peque�a ganancia desde el alineamiento del
cilindro. En discos mas viejos, es mas eficiente seleccionar los tama�os
de las particiones que son multiples exactos de su tama�o actual del cilindro.

Escoja su especificador de tama�o}

message defaultunit
{A no ser que haya especificado con 'M' (megabytes), 'G' (gigabytes), 'c'
(cilindros) o 's' sectores al final de la entrada, los tama�os y
compensaciones estan en %s.
}

message ptnsizes
{Ahora puede cambiar los tama�os para las particiones del sistema. Por
defecto se aloja todo el espacio a la particion root, sin embargo
podria querer separar /usr (archivos de sistema adicionales), /var
(archivos de log etc) o /home (directorios hogar de los usuarios).

El espacio libre sobrante sera a�adido a la partici�n marcada con '+'.
}

message ptnheaders
{
       MB         Cilindros	Sectores  Sistema de archivos 
}

message askfsmount
{�Punto de montaje?}

message askfssize
{�Tama�o para %s en %s?}

message askunits
{Cambiar unidades de entrada (sectores/cilindros/MB)}

message NetBSD_partition_cant_change
{particion NetBSD}

message Whole_disk_cant_change
{Todo el disco}

message Boot_partition_cant_change
{Partici�n de arranque}

message add_another_ptn
{A�adir una partici�n definida por el usuario}

message fssizesok
{Aceptar tama�o de particiones.  Espacio libre %d %s, %d particiones libres.}

message fssizesbad
{Reducir tama�o de particiones por %d %s (%d sectores).}

message startoutsidedisk
{El valor del comienzo que ha especificado est� mas all� del extremo del disco.
}

message endoutsidedisk
{Con este valor, el extremo de la partici�n est� mas all� del extremo del disco.
Su tama�o de la partici�n se ha truncado a %d %s. 

Presione enter para continuar
}

message fspart
{Ahora tenemos sus particiones BSD-disklabel: 
Esta es su �ltima oportunidad para cambiarlas.

}

message fspart_header
{  Inicio %3s Fin %3s   Tama�o %3s Tipo FS    Newfs Mont. Punto mont. 
   ---------- --------- ---------- ---------- ----- ----- -----------
}

message fspart_row
{%10d %9d %10d %-10s %-5s %-5s %s}

message show_all_unused_partitions
{Mostrar todas las particiones no usadas}

message partition_sizes_ok
{Tama�os de partici�n ok}

message edfspart
{Los valores actuales para la particion `%c' son, 
Seleccione el campo que desee cambiar:

                          MB cilindros  sectores
	             ------- --------- ---------
}

message fstype_fmt
{        FStype: %9s}

message start_fmt
{        inicio: %9u %8u%c %9u}

message size_fmt
{        tama�o: %9u %8u%c %9u}

message end_fmt
{           fin: %9u %8u%c %9u}

message bsize_fmt
{tama�o  bloque: %9d bytes}

message fsize_fmt
{   tama�o frag: %9d bytes}

message isize_fmt
{tam prom archi: %9d bytes (para n�mero de inodos)}
message isize_fmt_dflt
{tam prom archi:         4 fragmentos}

message newfs_fmt
{         newfs: %9s}

message mount_fmt
{        montar: %9s}

message mount_options_fmt
{   opc montaje: }

message mountpt_fmt
{ punto montaje: %9s}

message toggle
{Habilitar}

message restore
{Restaurar valores originales}

message Select_the_type
{Seleccione el tipo}

message other_types
{otros tipos}

message label_size
{%s
Valores especiales que pueden ser entrados para el valor del tama�o:
     -1:   usar hasta la parte final del disco NetBSD
   a-%c:   acabe esta particion donde la particion X empieza

tama�o (%s)}

message label_offset
{%s
Valores especiales que pueden ser entrados para el valor de compensado:
     -1:   empezar al principio de la parte NetBSD del disco
   a-%c:   empezar al final de la particion previa (a, b, ..., %c)

inicio (%s)}

message invalid_sector_number
{N�mero de sector malformado
}

message Select_file_system_block_size
{Seleccione tama�o de bloque del sistema de archivos}

message Select_file_system_fragment_size
{Seleccione tama�o de fragmento del sistema de archivos}

message packname
{Por favor entre un nombre para su disco NetBSD}

message lastchance
{Esta bien, ahora estamos preparados para instalar NetBSD en su disco duro (%s).
Nada se ha escrito todavia. Esta es su �ltima oportunidad para salir del proceso
antes de que nada sea cambiado.

�Deber�amos continuar?
}

message disksetupdone
{De acuerdo, la primera parte del procedimiento ha terminado.
Sysinst ha escrito un disklabel en el disco objetivo, y ha
newfs'ado y fsck'ado las nuevas particiones que ha especificado
para el disco objetivo. 

El paso siguiente es bajar y desempaquetar los archivos de
distribuci�n.

Presione <return> para proceder.
}

message disksetupdoneupdate
{De acuerdo, la primera parte del procedimiento ha terminado.
sysinst ha escrito el disklabel al disco, y fsck'ado las
nuevas particiones que ha especificado para el disco objetivo. 

El paso siguiente es bajar y desempaquetar los archivos de
distribucion.

Presione <return> para proceder.
}

message openfail
{No se ha podido abrir %s, el mensaje de error ha sido: %s.
}

message statfail
{No se pueden obtener las propiedades de %s, el mensaje de error ha sido: %s.
}

message unlink_fail
{No he podido eliminar %s, el mensaje de error ha sido: %s.
}

message rename_fail
{No he podido renombrar %s a %s, el mensaje de error ha sido: %s.
}

message deleting_files
{Como parte del proceso de actualizaci�n, lo siguiente tiene que ser eliminado:
}

message deleting_dirs
{Como parte del proceso de actualizaci�n, los siguientes directorios
tienen que ser eliminados (renombrar� los que no esten vacios):
}

message renamed_dir
{El directorio %s ha sido renombrado a %s porque no estaba vacio.
}

message cleanup_warn
{Limpieza de la instalaci�n existente fallida. Esto puede causar fallos
en la extracci�n del set.
}

message nomount
{El tipo de partici�n de %c no es 4.2BSD o msdos, por lo tanto no tiene
un punto de montaje.}

message mountfail
{montaje del dispositivo /dev/%s%c en %s fallida.
}

message extractcomplete
{Extracci�n de los sets seleccionados para NetBSD-@@VERSION@@ completa.
El sistema ahora es capaz de arrancar desde el disco duro seleccionado.
Para completar la instalaci�n, sysinst le dar� la oportunidad de
configurar algunos aspectos esenciales.
}

message instcomplete
{Instalaci�n de NetBSD-@@VERSION@@ completada. El sistema deber�a
arrancar desde el disco duro. Siga las instrucciones del documento
INSTALL sobre la configuraci�n final de su sistema. La pagina man
de afterboot(8) es otra lectura recomendada; contiene una lista de
cosas a comprobar despues del primer inicio completo.

Como minimo, debe editar /etc/rc.conf para cumplir sus necesidades.
Vea /etc/defaults/rc.conf para los valores por defecto.
}

message upgrcomplete
{Actualizaci�n a NetBSD-@@VERSION@@ completada. Ahora tendr� que
seguir las instrucciones del documento INSTALL en cuanto a lo que
usted necesita hacer para conseguir tener su sistema configurado
de nuevo para su situaci�n.
Recuerde (re)leer la pagina del man afterboot(8) ya que puede contener
nuevos apartados desde su ultima actualizaci�n.

Como minimo, debe editar rc.conf para su entorno local y cambiar
rc_configured=NO a rc_configured=YES o los reinicios se parar�n en
single-user, y copie de nuevo los archivos de password (considerando
nuevas cuentas que puedan haber sido creadas para esta release) si
estuviera usando archivos de password locales.
}


message unpackcomplete
{Desempaquetamiento de sets adicionales de NetBSD-@@VERSION@@ completado.
Ahora necesitara seguir las instrucciones en el documento INSTALL para
tener su sistema reconfigurado para su situaci�n.
La pagina de man afterboot(8) tambi�n puede serle de ayuda.

Como minimo, debe editar /etc/rc.conf para cumplir sus necesidades.
Vea /etc/defaults/rc.conf para los valores por defecto.
}

message distmedium
{Su disco ahora est� preparado para instalar el nucleo y los sets de
distribuci�n. Como aparece anotado en las notas INSTALL, tiene diversas
opciones. Para ftp o nfs, tiene que estar conectado a una red con acceso
a las maquinas apropiadas. Si no esta preparado para completar la
instalaci�n en este momento, deber� seleccionar "ninguno" y ser� retornado
al men� principal. Cuando est� preparado mas tarde, deber� seleccionar
"actualizar" desde el men� principal para completar la instalaci�n.
}

message distset
{La distribuci�n NetBSD est� dividida en una colecci�n de sets. Hay
algunos sets b�sicos que son necesarios para todas las instalaciones y
hay otros sets que no son necesarios para todas las instalaciones.
Deber� escoger para instalar todas (Instalaci�n completa) o
seleccionar desde los sets de distribuci�n opcionales.
}

message ftpsource
{Lo siguiente es el sitio ftp, directorio, usuario y password actual
listo para usar. Si el "usuario" es "ftp", entonces el password no ser�
necesario.

host:		%s 
dir base:	%s 
dir de sets:	%s 
usuario:	%s 
password:	%s 
proxy:		%s 
}

message email
{direcci�n de e-mail}

message dev
{dispositivo}

message nfssource
{Entre el host del nfs i el directorio del servidor donde est� localizada
la distribuci�n.
Recuerde, el directorio debe contener los archivos .tgz y debe ser
montable por nfs.

host:		%s
dir base:	%s
dir de sets:	%s
}

message nfsbadmount
{El directorio %s:%s no pudo ser montado por nfs.}

message cdromsource
{Entre el dispositivo de CDROM para ser usado y el directorio del CDROM
donde est� localizada la distribuci�n.
Recuerde, el directorio debe contener los archivos .tgz.

dispositivo:	%s
dir de sets:	%s
}

message localfssource
{Entre el dispositivo local desmontado y el directorio en ese dispositivo
donde est� localizada la distribuci�n.
Recuerde, el directorio debe contener los archivos .tgz.

dispositivo:	%s
sist de archiv:	%s
dir base:	%s
dir de sets:	%s
}

message localdir
{Entre el directorio local ya montado donde esta localizada la distribuci�n.
Recuerde, el directorio debe contener los archivos .tgz.

dir base:	%s 
dir de sets:	%s
}

message filesys
{sistema de archivos}

message cdrombadmount
{El CDROM /dev/%s no ha podido ser montado.}

message localfsbadmount
{%s no ha podido ser montado en el dispositivo local %s.}

message badlocalsetdir
{%s no es un directorio}

message badsetdir
{%s no contiene los sets de instalaci�n obligatorios etc.tgz 
y base.tgz.  �Est� seguro de que ha introducido el directorio
correcto?}

message nonet
{No puedo encontrar ninguna interfaz de red para usar con NetBSD.
Volver� al men� anterior.
}

message netup
{Las siguientes interfaces est�n activas: %s
�Conecta alguna de ellas al servidor requerido?}

message asknetdev
{He encontrado las siguientes interfaces de red: %s
\n�Cual deber�a usar?}

message badnet
{No ha seleccionado una interfaz de las listadas. Por favor vuelva a
intentarlo.
Las siguientes interfaces de red estan disponibles: %s
\n�Cual deber�a usar?}

message netinfo
{Para ser capaz de usar la red, necesitamos respuestas a lo siguiente:

}

message net_domain
{Su dominio DNS}

message net_host
{Su nombre del host}

message net_ip
{Su numero IPv4}

message net_ip_2nd
{Numero servidor IPv4}

message net_mask
{Mascara IPv4}

message net_namesrv6
{Servidor de nombres IPv6}

message net_namesrv
{Servidor de nombres IPv4}

message net_defroute
{Pasarela IPv4}

message net_media
{Tipo de medio de red}

message netok
{Ha entrado los siguientes valores.

Dominio DNS :		%s 
Nombre del Host:	%s
Interfaz primaria:	%s
Host IP:		%s
Mascara:		%s
Serv de nombres IPv4:	%s
Pasarela IPv4:		%s
Tipo de medio:		%s
}

message netok_slip
{Los siguientes valores son los que has metido. Son correctos?}

message netokv6
{IPv6 autoconf:		%s
Serv de nombres IPv6:	%s
}

message netok_ok
{�Son correctos?}

message netagain
{Por favor reentre la informaci�n sobre su red. Sus ultimas respuestas
ser�n las predeterminadas.

}

message wait_network
{
Espere mientras las interfaces de red se levantan.
}

message resolv
{No se ha podido crear /etc/resolv.conf.  Instalaci�n cancelada.
}

message realdir
{No se ha podido cambiar el directorio a %s: %s.  Instalaci�n
cancelada.
}

message ftperror
{ftp no ha podido bajar un archivo.
�Desea intentar de nuevo?}

message distdir
{�Que directorio deber�a usar para %s? }

message delete_dist_files
{�Quiere borrar los sets de NetBSD de %s?
(Puede dejarlos para instalar/actualizar un segundo sistema.)}

message verboseextract
{Durante el proceso de instalaci�n, �que desea ver cuando
cada archivo sea extraido?
}

message notarfile
{El set %s no existe.}

message notarfile_ok
{�Continuar extrayendo sets?}

message endtarok
{Todos los sets de distribuci�n han sido desempaquetados
correctamente.}

message endtar
{Ha habido problemas desempaquetando los sets de distribuci�n.
Su instalaci�n est� incompleta.

Ha seleccionado %d sets de distribuci�n. %d sets no se han
encontrado y %d han sido saltados despues de que ocurriera un
error. De los %d que se han intentado, %d se han desempaquetado
sin errores y %d con errores.

La instalaci�n est� cancelada. Por favor compruebe de nuevo su
fuente de distribuci�n y considere el reinstalar los sets desde
el men� principal.}

message abort
{Sus opciones han hecho imposible instalar NetBSD. Instalacion abortada.
}

message abortinst
{La distribuci�n no ha sido correctamente cargada. Necesitar� proceder
a mano. Instalaci�n abortada.
}

message abortupgr
{La distribuci�n no ha sido correctamente cargada. Necesitar� proceder
a mano. Instalaci�n abortada.
}

message abortunpack
{Desempaquetamiento de sets adicionales no satisfactoria. Necesitar�
proceder a mano, o escoger una fuente diferente para los sets de
esta release y volver a intentarlo.
}

message createfstab
{�Hay un gran problema!  No se puede crear /mnt/etc/fstab. �Saliendo!
}


message noetcfstab
{�Ayuda! No hay /etc/fstab en el disco objetivo %s. Abortando actualizaci�n.
}

message badetcfstab
{�Ayuda! No se puede analizar /etc/fstab en el disco objetivo %s.
Abortando actualizaci�n.
}

message X_oldexists
{No puedo dejar /usr/X11R6/bin/X como /usr/X11R6/bin/X.old, porque el
disco objetivo ya tiene un /usr/X11R6/bin/X.old. Por favor arregle esto
antes de continuar.

Una manera es iniciando una shell desde el menu de Utilidades, examinar
el objetivo /usr/X11R6/bin/X y /usr/X11R6/bin/X.old. Si
/usr/X11R6/bin/X.old es de una actualizaci�n completada, puede rm -f
/usr/X11R6/bin/X.old y reiniciar. O si /usr/X11R6/bin/X.old es de
una actualizacion reciente e incompleta, puede  rm -f /usr/X11R6/bin/X
y mv /usr/X11R6/bin/X.old a /usr/X11R6/bin/X.

Abortando actualizaci�n.}

message netnotup
{Ha habido un problema configurando la red. O su pasarela o su servidor
de nombres no son alcanzables por un ping. �Quiere configurar la red
de nuevo? (No le deja continuar de todos modos ni abortar el proceso
de instalaci�n.)
}

message netnotup_continueanyway
{�Le gustar�a continuar el proceso de instalaci�n de todos modos, y
asumir que la red est� funcionando? (No aborta el proceso de
instalaci�on.)
}

message makedev
{Creando nodos de dispositivo ...
}

message badfs
{Parece que /dev/%s%c no es un sistema de archivos BSD o el fsck
no ha sido correcto. La actualizaci�n ha sido abortada.  (Error
n�mero %d.)
}

message badmount
{Su sistema de archivos /dev/%s%c no ha sido montado correctamente.
Actualizaci�n abortada.}

message rootmissing
{ el directorio raiz objetivo no existe %s.
}

message badroot
{El nuevo sistema de archivos raiz no ha pasado la comprobaci�n b�sica.
 �Est� seguro de que ha instalado todos los sets requeridos? 

}

message fddev
{�Qu� dispositivo de disquette quiere usar? }

message fdmount
{Por favor inserte el disquette que contiene el archivo "%s". }

message fdnotfound
{No se ha encontrado el archivo "%s" en el disco.  Por favor inserte
el disquette que lo contenga.

Si este era el ultimo disco de sets, presione "Set acabado" para
continuar con el siguiente set, si lo hay.}

message fdremount
{El disquette no ha sido montado correctamente. Deberia:

Intentar de nuevo e insertar el disquette que contenga "%s".

No cargar ningun otro archivo de este set y continuar con el siguiente,
si lo hay.

No cargar ningun otro archivo desde el disquette y abortar el proceso.
}

message mntnetconfig
{�Es la informaci�n que ha entrado la adecuada para esta maquina
en operaci�n regular o lo quiere instalar en /etc? }

message cur_distsets
{Lo siguiente es la lista de sets de distribuci�n que ser� usada.

}

message cur_distsets_header
{   Set de distribuci�n     Selecc.
   ------------------------ --------
}

message set_base
{Base}

message set_system
{Sistema (/etc)}

message set_compiler
{Herramientas de Compilador}

message set_games
{Juegos}

message set_man_pages
{Paginas del Manual en Linea}

message set_misc
{Miscelaneos}

message set_text_tools
{Herramientas de Procesamiento de Texto}

message set_X11
{Sets de X11}

message set_X11_base
{X11 base y clientes}

message set_X11_etc
{Configuraci�n de X11}

message set_X11_fonts
{Fuentes de X11}

message set_X11_servers
{Servidores X11}

message set_X_contrib
{Clientes de X contrib}

message set_X11_prog
{Programaci�n de X11}

message set_X11_misc
{X11 Misc.}

message cur_distsets_row
{%-27s %3s\n}

message select_all
{Seleccione todos los sets anteriores}

message select_none
{Des-seleccione todos los sets anteriores}

message install_selected_sets
{Instalar sets seleccionados}

message tarerror
{Ha habido un error extrayendo el archivo %s. Esto significa
que algunos archivos no han sido extraidos correctamente y su sistema
no estar� completo.

�Continuar extrayendo sets?}

message must_be_one_root
{Debe haber una sola partici�n marcada para ser montada en '/'.}

message partitions_overlap
{las particiones %c y %c se solapan.}

message edit_partitions_again
{

Puede editar la tabla de particiones a mano, o dejarlo y retornar al
men� principal.

�Editar la tabla de particiones de nuevo?}

message not_regular_file
{El archivo de configuraci�n %s no es un archivo regular.\n}

message out_of_memory
{Sin memoria (malloc fallido).\n}

message config_open_error
{No se ha podido abrir el archivo de configuraci�n %s\n}

message config_read_error
{No se ha podido leer el archivo de configuraci�n %s\n}

message cmdfail
{Comando
	%s
fallido. No puedo continuar.}

message upgradeparttype
{La unica partici�n adecuada que se ha encontrado para la instalaci�n de
NetBSD es del tipo viejo de partici�n de NetBSD/386BSD/FreeBSD. �Quiere
cambiar el tipo de esta partici�n al nuevo tipo de partici�n de
solo-NetBSD?}

message choose_timezone
{Por favor escoja la zona horaria que se ajuste mejor de la siguiente
lista.
Presione RETURN para seleccionar una entrada.
Presione 'x' seguido de RETURN para salir de la selecci�n de la
zona horaria.

 Defecto:	%s 
 Seleccionado:	%s 
 Hora local: 	%s %s 
}

message tz_back
{ Volver a la lista principal de zona horaria}

message choose_crypt
{Por favor seleccione el cifrador de password a usar. NetBSD puede ser
configurado para usar los esquemas DES, MD5 o Blowfish.

El esquema tradicional DES es compatible con la mayoria de sistemas operativos
tipo-Unix, pero solo los primeros 8 car�cteres de cualquier password ser�n
reconocidos.
Los esquemas MD5 y Blowfish permiten passwords mas largos, y hay quien
discutir�a si es mas seguro.

Si tiene una red o pretende usar NIS, por favor considere las capacidades
de otras maquinas en su red.

Si est� actualizando y le gustaria mantener la configuraci�n sin cambios,
escoja la ultima opci�n "no cambiar".
}

message swapactive
{El disco que ha seleccionado tiene una partici�n swap que puede que est�
en uso actualmente si su sistema tiene poca memoria. Como est� a punto
de reparticionar este disco, esta partici�n swap ser� desactivada ahora.
Por favor tenga en cuenta que esto puede conducir a problemas de swap.
Si obtuviera algun error, por favor reinicie el sistema e intente de nuevo.}

message swapdelfailed
{sysinst ha fallado en la desactivaci�n de la partici�n swap en el disco
que ha escogido para la instalaci�n. Por favor reinicie e intente de nuevo.}

message rootpw
{El password de root del nuevo sistema instalado no ha sido fijado todavia,
y por eso est� vacio. �Quiere fijar un password de root para el sistema ahora?}

message rootsh
{Ahora puede seleccionar que shell quiere usar para el usuario root. Por
defecto es /bin/csh, pero podria preferir otra.}

message postuseexisting
{
No olvide comprobar los puntos de montaje para cada sistema de archivos
que vaya a ser montado. Presione <return> para continuar.
}

message no_root_fs
{
No hay un sistema de archivos raiz definido. Necesitara al menos un punto
de montaje con "/".

Presione <return> para continuar.
}

message slattach {
Especifique los parametros de slattach
}

message Pick_an_option {Seleccione una opci�n para activar/desactivar.}
message Scripting {Scripting}
message Logging {Logging}

message Status  { Estado: }
message Command {Comando: }
message Running {Ejecutando}
message Finished {Acabado}
message Command_failed {Comando fallido}
message Command_ended_on_signal {Comando terminado en se�al}

message NetBSD_VERSION_Install_System {Sistema de Instalaci�n de NetBSD-@@VERSION@@}
message Exit_Install_System {Salir del Sistema de Instalaci�n}
message Install_NetBSD_to_hard_disk {Instalar NetBSD al disco duro}
message Upgrade_NetBSD_on_a_hard_disk {Actualizar NetBSD en un disco duro}
message Re_install_sets_or_install_additional_sets {Re-instalar sets o instalar sets adicionales}
message Reboot_the_computer {Reiniciar la computadora}
message Utility_menu {Men� de utilidades}
message NetBSD_VERSION_Utilities {Utilidades de NetBSD-@@VERSION@@}
message Run_bin_sh {Ejecutar /bin/sh}
message Set_timezone {Ajustar zona horaria}
message Configure_network {Configurar red}
message Partition_a_disk {Particionar un disco}
message Logging_functions {Funciones de loggeo}
message Halt_the_system {Parar el sistema}
message yes_or_no {�si o no?}
message Hit_enter_to_continue {Presione enter para continuar}
message Choose_your_installation {Seleccione su instalaci�n}
message Set_Sizes {Ajustar tama�os de particiones NetBSD}
message Use_Existing {Usar tama�os de particiones existentes}
message Megabytes {Megabytes}
message Cylinders {Cilindros}
message Sectors {Sectores}
message Select_medium {Seleccione medio}
message ftp {FTP}
message http {HTTP}
message nfs {NFS}
message cdrom {CD-ROM / DVD}
message floppy {Disquette}
message local_fs {Sistema de Archivos Desmontado}
message local_dir {Directorio Local}
message Select_your_distribution {Seleccione su distribuci�n}
message Full_installation {Instalaci�n completa}
message Custom_installation {Instalaci�n personalizada}
message Change {Cambiar}
message hidden {** oculto **}
message Host {Host}
message Base_dir {Directorio base}
message Set_dir {Directorio de sets}
message Directory {Directorio}
message User {Usuario}
message Password {Password}
message Proxy {Proxy}
message Get_Distribution {Bajar Distribuci�n}
message Continue {Continuar}
message What_do_you_want_to_do {�Qu� desea hacer?}
message Try_again {Reintentar}
message Give_up {Give up}
message Ignore_continue_anyway {Ignorar, continuar de todos modos}
message Set_finished {Finalizar}
message Abort_install {Abortar instalaci�n}
message Password_cipher {Cifrador de password}
message DES {DES}
message MD5 {MD5}
message Blowfish_2_7_round {Blowfish 2^7 round}
message do_not_change {no cambiar}
message Device {Dispositivo}
message File_system {Sistema de archivos}
message Select_IPv6_DNS_server {  Seleccione servidor DNS de IPv6}
message other {otro }
message Perform_IPv6_autoconfiguration {�Realizar autoconfiguraci�n IPv6?}
message Perform_DHCP_autoconfiguration {�Realizar autoconfiguraci�n DHCP ?}
message Root_shell {Root shell}
message Select_set_extraction_verbosity {Seleccione detalle de extracci�n de sets}
message Progress_bar {Barra de progreso (recomendado)}
message Silent {Silencioso}
message Verbose {Listado de nombres de archivo detallado (lento)}

.if AOUT2ELF
message aoutfail
{El directorio donde las librerias compartidas antiguas a.out deberian ser
movidas no ha podido ser creado. Por favor intente el proceso de actualizacion
de nuevo y asegurese de que ha montado todos los sistemas de archivos.}

message emulbackup
{El directorio /emul/aout o /emul de su sistema tiene un enlace simbolico
apuntando a un sistema de archivos desmontado. Se le ha dado la extension '.old'.
Once you bring your upgraded system back up, you may need to take care
of merging the newly created /emul/aout directory with the old one.
}
.endif
