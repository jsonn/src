/*	$NetBSD: msg.mbr.es,v 1.2.2.4 2005/10/07 11:48:55 tron Exp $	*/

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

/* MBR Message catalog -- Spanish, i386 version */

/* NB: Lines ending in spaces force line breaks */

message fullpart
{Vamos a instalar NetBSD en el disco %s.

NetBSD requiere una sola partici�n en la tabla de particiones MBR del disco,
que es partida adecuadamente por el disklabel de NetBSD.
NetBSD tambi�n puede acceder a sistemas de archivos en otras particiones MBR.

Si selecciona 'Usar todo el disco' entonces el contenido previo del
disco ser� sobreescrito y una sola partici�n MBR sera usada para cubrir
todo el disco.
Si quiere instalar mas de un sistema operativo entonces edite la
tabla de particiones MBR y cree una partici�n para NetBSD.

Unos pocos MB son necesarios para una instalaci�n basica, pero deber�
alojar espacio extra para programas adicionales y ficheros de usuario.
Proporcione al menos 5GB si quiere construir el propio NetBSD.
}

message Select_your_choice
{�Que desear�a hacer?}
message Use_only_part_of_the_disk
{Editar la tabla de particiones MBR}
message Use_the_entire_disk
{Usar todo el disco}

/* the %s's will expand into three character strings */
message part_header
{   Tama�o total del disco %d %s.

.if BOOTSEL
   Inicio(%3s) Tama�o(%3s) Opc Tipo                    Bootmenu
   ----------- ----------- --- ----------------------- --------
.else
   Inicio(%3s) Tama�o(%3s) Opc Tipo
   ----------- ----------- --- ----------------
.endif

}

message part_row_used
{%10d %10d %c%c%c}

message noactivepart
{No ha marcado una partici�n activa. Esto podr�a causar que su sistema
no arrancara correctamente. �Deber�a estar la partici�n NetBSD marcada
como activa?}

message setbiosgeom
{
Ser� preguntado por la geometria.
Por favor introduzca el n�mero de sectores por pista (maximo 63)
y n�mero de cabezales (maximo 256) que la BIOS usa para acceder al disco.
El n�mero de cilindros ser� calculado desde el tama�o del disco.

}

message nobiosgeom
{sysinst no ha podido determinar la geometr�a del disco de la BIOS.
La geometr�a fisica es de %d cilindros %d sectores %d cabezales\n}

message biosguess
{Usando la informaci�n ya en disco, mi mejor estimaci�n para la geometr�a
de la BIOS es de %d cilindros %d sectores %d cabezales\n}

message realgeom
{geom real: % cil, %d cabez, %d sec  (NB: solo para comparaci�n)\n}

message biosgeom
{geom BIOS: %d cil, %d cabez, %d sec\n}

message ovrwrite
{Actualmente su disco tiene una partici�n no-NetBSD. �Realmente quiere
sobreescribir dicha partici�n con NetBSD?
}

message Partition_OK
{Partici�n OK}

message ptn_type
{      tipo: %s}
message ptn_start
{    inicio: %d %s}
message ptn_size
{    tama�o: %d %s}
message ptn_end
{       fin: %d %s}
message ptn_active
{    activa: %s}
message ptn_install
{  instalar: %s}
.if BOOTSEL
message bootmenu
{  bootmenu: %s}
message boot_dflt
{   defecto: %s}
.endif

message get_ptn_size {%stama�o (maximo %d %s)}
message Invalid_numeric {N�mero inv�lido: }
message Too_large {Demasiado grande: }
message Space_allocated {Espacio ubicado: }
message ptn_starts {Espacio en %d..%d %s (tama�o %d %s)\n}
message get_ptn_start {%s%sInicio (en %s)}
message get_ptn_id {Tipo de partici�n (0..255)}
message No_free_space {Sin espacio libre}
message Only_one_extended_ptn {Solamente puede haber una partici�n extendida}

message editparttable
{La tabla de particiones MBR es mostrada a continuaci�n. 
Opcn: a => Partici�n activa,
.if BOOTSEL
d => defecto bootselect,
.endif
I => Instalar aqu�. 
Seleccione la partici�n que desee editar:

}

message Partition_table_ok
{Tabla de particiones OK}

message Delete_partition
{Borrar partici�n}
message Dont_change
{No cambiar}
message Other_kind
{Otra, introducir n�mero}

message reeditpart
{

�Quiere reeditar la tabla de particiones MBR (o abandonar la instalaci�n)?
}

message nobsdpart
{No hay ninguna partici�n NetBSD en la tabla de particiones MBR.}

message multbsdpart
{Hay multiples particiones NetBSD en la tabla de particiones MBR.
Deber�a seleccionar la opci�n "instalar" en la que quiera usar. }

message dofdisk
{Configurando la tabla de particiones DOS ...
}

message wmbrfail
{Reescritura de MBR fallida. No puedo continuar.}

.if 0
.if BOOTSEL
message Set_timeout_value
{Elija el tiempo de espera}

message bootseltimeout
{Tiempo de espera del menu: %d\n}

.endif
.endif

