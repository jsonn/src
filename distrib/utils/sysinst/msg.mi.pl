/*	$NetBSD: msg.mi.pl,v 1.1.2.1 2002/06/21 16:03:48 lukem Exp $	*/
/*	Based on english version: */
/*	NetBSD: msg.mi.en,v 1.86 2002/04/04 14:26:44 ad Exp 	*/

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

/* MI Message catalog -- english, machine independent */

message usage
{uzycie: sysinst [-r wersja] [-f plik-definicji]
}

message newline {\n}

message yes {Tak}

message no {Nie}

message install
{zainstalowac}

message reinstall
{przeinstaluj pakiety dla}

message upgrade
{zaktualizowac}


message hello
{Witaj w sysinst,  systemie instalacyjnym  NetBSD-@@VERSION@@.
To oparte na menu narzedzie zostalo stworzone aby pomoc ci zainstalowac
NetBSD na twardym dysku, lub zaktualizowac istniejacy system NetBSD,
zuzywajac minimum czasu.  W ponizszych menu, mozesz zmienic aktualne
ustawienia poprzez naciskanie klawiszy (a, b, c, ...). Klawisze strzalek
takze moga dzialac.  Aktywujesz ustawienie poprzez nacisniecie ENTER.

}

message thanks
{Dziekujemy za uzywanie NetBSD!
}

message installusure
{Zdecydowales sie zainstalowac NetBSD na twardym dysku. Spowoduje to zmiane
informacji na twoim dysku. Powinienes zrobic pelny backup danych przed
rozpoczeciem tej procedury!  Zostana wykonane nastepujace czynnosci: 
	a) Podzial dysku twardego
	b) Stworzenie nowych systemow plikow BSD
	c) Wgranie i zainstalowanie pakietow dystrybucji

(Po wprowadzeniu informacji o partycjach, ale jeszcze zanim twoj dysk
zostanie zmieniony, bedziesz mial mozliwosc przerwac ta procedure.)

Czy kontynuowac?
}

message upgradeusure
{Ok, zaktualizujmy NetBSD na twoim dysku.  Jak zawsze, spowoduje to
zmiane informacji na twoim dysku. Powinienes zrobic pelny backup danych
przed rozpoczeciem!  Czy napewno chcesz zaktualizowac NetBSD?
(Jest to ostatnie ostrzezenie zanim zacznie sie modyfikacja danych na
twoich dyskach.)
}

message reinstallusure
{Ok, rozpakujmy pakiety dystrybucyjne NetBSD na bootowalny twardy dysk.
Ta procedura tylko sciaga i rozpakowuje pakiety na pre-partycjonowany
bootowalny dysk. Nie nazywa dyskow, aktualizuje bootblokow, lub zapisuje
istniejacej konfiguracji.   (Wyjdz i wybierz `instaluj' lub
`aktualizuj' jesli chcesz to zrobic.) Powinienes wykonac `instaluj' lub 
`aktualizuj' przed rozpoczeciem tej procedury!

Czy napewno chcesz przeinstalowac pakiety dystrybucjne NetBSD?
(Jest to ostatnie ostrzezenie zanim zacznie sie modyfikacja danych na
twoich dyskach.)
}


message nodisk
{Nie moge znalezc zadnych twardych dyskow do uzycia z NetBSD. Zostaniesz
przeniesiony do menu glownego.
}

message onedisk
{Znalazlem tylko jeden dysk, %s.  Dlatego rozumiem, ze chcesz %s
NetBSD wlasnie na nim.
}

message askdisk
{Znalazlem nastepujace dyski: %s
\nNa ktorym z nich chcesz zainstalowac NetBSD? }

message badname
{Nie wybrales dysku z listy.  Sprobuj jeszcze raz.
Nastepujace dyski sa dostepne: %s
\nNa ktorym z nich chcesz zainstalowac NetBSD? }

message disktoosmall
{Twoj twardy dysk jest zbyt maly dla standardowej instalacji. Bedziesz musial
podac rozmiary partycji recznie.
}

message pleasemountroot
{Glowny dysk nie jest zamontowany. Zamontuj go.

Wybrany przez ciebie dysk docelowy %s jest takze aktualnym glownym dyskiem.
Musze wiedziec czy aktualnie dzialam poza docelowym dyskiem (%sa), czy 
poza alternatywnym (powiedzmy, w %sb, twojej partycji wymiany).
Nie moge tego stwierdzic dopoki nie zamontujesz glownej partycji z ktorej uruchomiles system zapis/odczyt (np 'mount /dev/%sb /').

Przerywam i wracam do glownego menu, abys mogl uruchomic powloke.
}

message cylinders
{cylindry}

message heads
{glowice}

message sectors
{sektory}

message size
{rozmiar}

message start
{start}

message offset
{przesuniecie}

message bsize
{rozmiar bloku}

message fsize
{rozmiar frag}

message mountpoint
{punkt montazu}

message cylname
{cyl}

message secname
{sek}

message megname
{MB}

message layout
{NetBSD uzywa BSD disklabel aby pociac czesc dysku NetBSD na kilka  
partycji BSD.  Musisz teraz skonfigurowac BSD disklabel.
Masz kilka mozliwosci. Sa one opisane ponizej. 
-- Standard: partycje BSD disklabel sa ustawiane przez ten program. 
-- Standard z Xami: podwojona partycja wymiany, miejsce na binaria X.
-- Wlasna: sam ustawiasz rozmiary wszystkich partycji BSD disklabel. 
-- Uzyj istniejacych: Uzywa aktualnych partycji. Musisz je zamontowac.

Dysk NetBSD to %.2f Megabajtow. 
Standard wymaga przynajmniej %.2f Megabajtow.
Standard z Xami wymaga przynajmniej %.2f Megabajtow.
}

message sizechoice
{Zdecydowales sie podac rozmiary partycji (albo dla BSD disklabel,
lub na niektorych portach, dla plastrow MBR). Musisz najpierw wybrac
jednostke rozmiaru. Wybranie Megabajtow nada partycji rozmiar bliski
twojemu wyborowi, ale dopasowany do granic cylindrow. Wybranie sektorow
pozwoli ci na bardziej dokladne okreslenie rozmiarow. Na nowych dyskach ZBR,
rozmiar cylindra zmienia sie w zaleznosci od miejsca na dysku, jest wiec
niewielka korzysc z dopasowania cylindrow. Na starszych dyskach bardziej
wydajne jest podawanie rozmiaru dysku, ktore sa wielokrotnoscia aktualnego
rozmiaru cylindra.
}

message defaultunit
{Jezeli nie wybrales 'M' (megabajty), 'c' (cylindry) lub 's' sektory
rozmiary i przesuniecia podawane sa w %s.
}

message startoutsidedisk
{Wartosc poczatkowa ktora podales jest poza koncem dysku.
}

message endoutsidedisk
{Przy tej wartosci, koniec partycji znajduje sie poza koncem dysku. Rozmiar
twojej partycji zostal zmniejszony do %d %s.
}

message fspart
{Mamy teraz twoje  partycje BSD-disklabel jako (Rozmiar i Przesuniecie w %s):

}

message fspart_header
{   Rozmiar  Przesun.   Koniec   Typ SP Bsize Fsize  Ochrona Punkt mont.
   --------- --------- --------- ------ ----- ----- -------- -----------
}

message fspart_row_start
{%c: %-9d %-9d %-9d %-6s }

message fspart_row_end_bsd
{%-5d %-5d %-8s %s\n}

message fspart_row_end_msdos
{                     %s\n}

message fspart_row_end_other
{\n}

message edfspart
{Powinienes najpierw ustawic rodzaj systemu plikow (SP). Pozniej inne wartosci.

Aktualne wartosci dla partycji %c:

}

message not42bsd
{Partycja %c nie jest typu 4.2BSD i dlatego nie ma mozliwosci ustawienia
rozmiaru bloku i frag.
}

message packname
{Podaj nazwe dla swojego dysku NetBSD}

message lastchance
{Ok, jestesmy teraz gotowi zainstalowac NetBSD na twoim dysku (%s). Nic 
nie zostalo jeszcze zapisane. Masz teraz ostatnia szanse na przerwanie tego
procesu poki nic nie zostalo jeszcze zmienione.

Czy kontynuowac ?
}

message disksetupdone
{Okej, pierwsza czesc procedury zostala zakonczona. Sysinst zapisal
disklabel na dysk doceloway, oraz utworzyl system plikow i sprawdzil
nowe partycje, ktore podales jako docelowe.

Kolejny krok to sciagniecie i rozpakowanie pakietow dystrybucji.
Nacisnij <enter> aby kontynuowac.
}

message disksetupdoneupdate
{Okej, pierwsza czesc procedury zostala zakonczona. Sysinst zapisal
disklabel na dysk docelowy, oraz sprawdzil nowe partycje, ktore
podales jako docelowe.

Kolejny krok to sciagniecie i rozpakowanie pakietow dystrybucji.
Nacisnij <enter> aby kontynuowac.
}

message openfail
{Nie moglem otworzyc %s, blad: %s.
}

message statfail
{Nie moglem pobrac wlasciwosci %s, blad: %s.
}

message unlink_fail
{Nie moglem skasowac %s, blad: %s.
}

message rename_fail
{Nie moglem zmienic nazwy %s na %s, blad: %s.
}

message deleting_files
{Jako czesc procedury aktualizacji, ponizsze pliki musza zostac usuniete:
}

message deleting_dirs
{Jako czesc procedury aktualizacji, ponizsze katalogi musza zostac usuniete:
(Zmienie nazwy tych, ktore nie sa puste):
}

message renamed_dir
{Katalog %s zostal przezwany na %s poniewaz nie byl pusty.
}

message cleanup_warn
{Wyczyszczenie istniejacej instalacji nie powiodlo sie. Moze to spowodowac
nie powodzenie rozpakowywania pakietow.
}

message nomount
{Typ partycji %c to nie 4.2BSD lub msdos i dlatego nie ma ona punktu
montazu.}

message mountfail
{zamontowanie urzadzenia %s na %s nie powiodlo sie.
}

message dotar
{Wypelnianie dysku binariami bootowalnymi i plikami konfiguraciyjnymi...\n}

message dotarfail
{Nie powiodla sie instalacja binarii bootowalnych oraz plikow konfiguracyjnych.
Nie moge kontynuowac ...}

message extractcomplete
{Rozpakowywanie wybranych pakietow dla NetBSD-@@VERSION@@ zakonczone.
System moze sie teraz uruchomic z wybranego twardego dysku. Aby zakonczyc
instalacje, sysinst da ci mozliwosc skonfigurowania kilku istotnych rzeczy.
}

message instcomplete
{Instalacja NetBSD-@@VERSION@@ zostala zakonczona. System powinien
uruchomic sie z twardego dysku. Wykonaj polecenia zawarte w pliku
INSTALL o koncowej konfiguracji systemu.

Przynajmniej powinienes wyedytowac /etc/rc.conf aby odpowiadal twoim
potrzebom. Przegladnij /etc/defaults/rc.conf aby poznac domyslne wartosci.
}

message upgrcomplete
{Aktualizacja NetBSD-@@VERSION@@ zostala zakonczona. Bedziesz teraz
musial wykonac polecenia zawarte w pliku INSTALL, aby uzyskac system 
odpowiadajacy twoim potrzebom. Twoj stary katalog /etc zostal zapisany
jako /etc.old.

Musisz przynajmniej dostosowac rc.conf do swojego lokalnego srodowiska
i zmienic rc_configured=NO na rc_configured=YES inaczej start systemu
zatrzyma sie na trybie jednego-uzytkownika, oraz skopiowac spowrotem
pliki z haslami (biorac pod uwage nowe konta systemowe ktore mogly
zostac utworzone dla tej wersji), jesli uzywales lokalnych plikow hasel.
}


message unpackcomplete
{Rozpakowywanie dodatkowych pakietow NetBSD-@@VERSION@@ zostalo zakonczone. 
Rozpakowywanie nadpisalo docelowy /etc. Jakikolwiek /etc.old zapisany
przez wczesniejsza aktualizacje zostal nietkniety. Musisz teraz wykonac
polecenia zawarte w pliku INSTALL aby przekonfigurowac system do swoich
potrzeb.

Musisz przynajmniej dostosowac rc.conf do swojego lokalnego srodowiska
i zmienic rc_configured=NO na rc_configured=YES inaczej start systemu
zatrzyma sie na trybie jednego-uzytkownika.
}

message distmedium
{Twoj dysk jest teraz gotowy na zainstalowanie jadra oraz pakietow
dystrybucyjnych. Jak napisano w pliku INSTALL masz terz kilka opcji. Dla
ftp lub nfs, musisz byc podlaczony do sieci z dostepem do odpowidnich
maszyn. Jesli nie jestes gotowy aby zakonczyc instalacje teraz, mozesz
wybrac "none" i zostaniesz przeniesiony do glownego menu. Kiedy bedziesz
juz pozniej gotowy, mozesz wybrac "aktualizuj" z glownego menu, aby
zakonczyc instalacje. 
}

message distset
{Dystrybucja NetBSD jest rozbita w kolekcje pakietow dystrybucyjnych.
Czesc z nich to pakiety podstawowe wymagane przez wszystkie instalacje,
a czesc nie jest przez wszystkie wymagana. Mozesz zainstalowac je
wszystkie (Pelna instalacja) lub wybrac z opcjonalnych pakietow.
}

message ftpsource
{Ponizej masz site ftp, katalog, uzytkownika, oraz haslo gotowe do uzycia.
Jesli "uzytkownik" to "ftp", wtedy haslo nie jest wymagane.

host:		%s 
katalog:	%s 
uzytkownik:	%s
haslo:		%s 
proxy:		%s 
}

message host
{host}

message dir
{katalog}

message user
{uzytkownik}

message passwd
{haslo}

message proxy
{proxy}

message email
{adres e-mail}

message dev
{urzadzenie}

message nfssource
{Wprowadz hosta nfs oraz katalog gdzie znajduje sie dystrybucja.
Pmietaj, ze katalog musi zawierac pliki .tgz oraz, ze musi byc
dostepny via nfs.

host:		%s 
katalog:	%s 
}

message nfsbadmount
{Katalog %s:%s jest niedostepny dla nfs.}

message cdromsource
{Podaj urzadzenie CDROM oraz katalog na CDROMie, w ktorym znajduje sie
dystrybucja. Pamietaj, ze katalog musi zawierac pliki .tgz.

urzadzenie:	%s
katalog:	%s
}

message localfssource
{Podaj niezamontowane lokalne urzadzenie oraz katalog na nim, gdzie
znajduje sie dystrybucja. Pamietaj, ze katalog musi zawierac pliki .tgz.

urzadzenie:	%s
system plikow:	%s
katalog:	%s
}

message localdir
{Podaj aktualnie zamontowany lokalny katalog, gdzie znajduje sie
dystrybucja. Pamietaj, ze katalog musi zawierac pliki .tgz.

katalog:	%s
}

message filesys
{system plikow}

message cdrombadmount
{CDROM nie moze zostac zamontowany na %s.}

message localfsbadmount
{%s nie mogl byc zamontowany na lokalnym urzadzeniu %s.}

message badlocalsetdir
{%s nie jest katalogiem}

message badsetdir
{%s nie zawiera wymaganych pakietow instalacyjnych etc.tgz, 
base.tgz.  Jestes pewien, ze podales dobry katalog ?}

message nonet
{Nie znalazlem zadnych interfejsow sieciowych do uzycia z NetBSD.
Zostaniesz przeniesiony do glownego menu.
}

message asknetdev
{Znalazlem nastepujace interfejsy sieciowe : %s
\nKtorego urzadzenia mam uzyc?}

message badnet
{Nie wybrales zadnego z podanych urzadzen sieciowych. Sprobuj jeszcze raz.
Nastepujace urzadzenie sieciowe sa dostepne: %s
\nKtorego urzadzenia mam uzyc?}

message netinfo
{Aby mozna bylo uzywac sieci, potrzebujemy odpowiedzi na ponizsze pytania:

}

message net_domain
{Twoja domena DNS}

message net_host
{Twoja nazwa hosta}

message net_ip
{Twoj adres IPv4}

message net_mask
{Maska podsieci IPv4}

message net_namesrv6
{Serwer nazw IPv6}

message net_namesrv
{Serwer nazw IPv4}

message net_defroute
{bramka IPv4}

message net_media
{Medium sieciowe}

message netok
{Ponizej sa wartoscki, ktore wprowadziles.  Czy sa poprawne?

Domena DNS:		%s 
Nazwa hosta:		%s
Podstawowy interfejs:	%s 
Adres IP:		%s 
Maska podsieci:		%s
Serwer nazw IPv4:	%s 
Bramka IPv4:		%s 
Medium sieciowe:	%s
}

message netokv6
{Autkonfiguracja IPv6:	%s 
Serwer nazw IPv6:	%s 
}

message netagain
{Wprowadz jeszcze raz informacje o twojej sieci. Twoje ostatnie odpowiedzi
beda domyslnymi wartosciami.

}

message resolv
{Nie moglem utworzyc /etc/resolv.conf.  Instalacja przerwana.
}

message realdir
{Nie moglem przejsc do katalogu %s: %s. Instalacja przerwana.
}

message ftperror_cont
{Ftp wykrylo blad. Nacisnij <enter> aby kontynuowac.}

message ftperror
{Ftp nie moze sciagnac pliku. Czy chcesz sprobowac jeszcze raz?}

message distdir
{Jakiego katalogu powinienem uzyc dla %s? }

message verboseextract
{Czy w trakcie rozpakowywania plikow, chcesz widziec nazwe aktualnie
wypakowywanego pliku ?
}

message badls
{Nie moge uruchomic /bin/ls.  Ten blad nie powinien sie wydarzyc.
Instalacja przerwana.
}

message notarfile
{Pakiet %s nie istnieje.

Kontynuowac rozpakowywanie pakietow?}

message endtarok
{Wszystkie wybrane pakiety dystrybucji zostaly rozpakowane.}

message endtar
{Wystapil blad w trackie rozpakowywania pakietow.
Twoja instalacja jest niekompletna.

Wybrales %d pakietow dystrybucyjnych.  %d pakiety nie zostaly znalezione
i %d zostalo pominietych z powodu bledow. Z  %d wyprobowanych,
%d rozpakowalo sie bez bledow i %d z bledami.

Instalacja zostala przerwana. Sprawdz zrodlo swojej dystrybucji i rozwaz
reinstalacje pakietow z glownego menu.}

message abort
{Wybrane przez ciebie opcje spowodowaly, ze zainstalowanie NetBSD jest
nie mozliwe. Instalacja zostala przerwana.
}

message abortinst
{Dystrybucja nie zostala pomyslnie wgrana. Bedziesz musial zrobic to recznie.
Instalacja zostala przerwana.
}

message abortupgr
{Dystrybucja nie zostala pomyslnie wgrana. Bedziesz musial zrobic to recznie.
Aktualizacja zostala przerwana.
}

message abortunpack
{Rozpakowanie dodatkowych pakietow nie udalo sie. Bedziesz musial
to zrobic recznie, albo wybierz inne zrodlo pakietow i sprobuj ponownie.
}

message anonprog
{sysinst: uruchamanie "%s"
}

message anonprogfailed
{
Program "%s" zakonczyl sie nie spodziewanie z kodem bledu: %s.

Stalo sie to prawdopodobnie poprzez wybranie nieprawidlowej opcji
instalacji pierwszego-poziomu---jak np proba Aktualizacji na pustym
dysku lub proba nowej Instalacji na aktualnie dzialajacym systemie.
Moze to byc tez wina zle skomponowanego miniroot. Cokolwiek jest
przyczyna, sysinst nie oczekiwal w tym miejscu zadnych bledow i
z tego powodu instalacja prawie napewno sie nie powiodla.

Sprawdz informacje o bledach i kontynuuj z *ekstremalna* ostroznoscia.
Nacisnij <enter> aby kontynuowac.}

message progdie
{
sysinst: Wykonywanie "%s" nieoczekiwanie sie niepowiodlo z kodem %s.
Nie mozna odzyskac, zaniechanie.
}

message createfstab
{Pojawil sie powazny problem! Nie mozna utworzyc /mnt/etc/fstab. Spadamy!
}


message noetcfstab
{Pomocy! Na dysku docelowym %s nie ma /etc/fstab. Przerywamy aktualizacje. 
}

message badetcfstab
{Pomocy! Nie moge przeczytac /etc/fdstab na dysku %s. Przerywamy aktualizacje.
}

message etc_oldexists
{Nie moge zapisac /etc jako /etc.old, poniewaz docelowy dysk juz posiada
/etc.old. Napraw to przed kontynuacja.

Jedyna mozliwosc to uruchomienie powloki z menu Narzedziowego,
sprawdzenie docelowego /etc i /etc.old. Jesli /etc.old pochodzi z
zakonczonej aktualizacji mozesz to usunac (rm -rf /etc.old), a potem
zresetowac komputer. Albo jesli /etc.old jest z aktualnej niekompletnej
aktualizacji mozesz usunac /etc (rm -rf /etc) a potem przeniesc /etc.old
na /etc (mv /etc.old /etc).

Przerywamy aktualizacje.}

message X_oldexists
{Nie moge zapisac /usr/X11R6/bin/X jako /usr/X11R6/bin/X.old, poniewaz
na docelowym dysku jest juz /usr/X11R6/bin/X.old. Napraw to przed kontynuacja.

Jedyny sposob to uruchomic powloke z menu Narzedziowego i sprawdzic
docelowe /usr/X11R6/bin/X oraz /usr/X11R6/bin/X.old. Jesli
/usr/X11R6/bin/X.old pochodzi z zakonczonej aktualizacji, mozesz usunac
/usr/X11R6/bin/X.old i zrobic restart. Albo jesli /usr/X11R6/bin/X.old
pochodzi z aktualnej niekompletnej aktualizacji, mozesz usunac
/usr/X11R6/bin/X i przeniesc /usr/X11R6/bin/X.old na /usr/X11R6/bin/X.

Przerywamy aktualizacje.}

message netnotup
{Pojawil sie problem z konfiguracja twojej sieci. Albo twoja bramka
albo serwer nazw nie byl osiagalny przez ping. Czy chcesz skonfigurowac
siec jeszcze raz? (Nie pozwala ci kontynuowac lub przerwac instalacje.)
}

message netnotup_continueanyway
{Czy chcesz kontynuowac proces instalacji i zalozyc, ze twoja siec dziala?
(Nie przerywa proces instalacji.)
}

message makedev
{Tworzenie plikow urzadzen ...
}

message badfs
{Wyglada na to, ze %s%s nie jest systemem plikow BSD albo nie powiodlo sie
jego sprawdzenie. Aktualizacja zostala przerwana. (Blad numer %d.)
}

message badmount
{System plikow %s%s nie zostal pomyslnie zamontowany. Aktualizacja przerwana.}

message upgrinode
{System plikow, %s, uzywa starego formatu inode. Jesli uzywasz
tylko NetBSD na tych systemach plikow, jest zalecane aby to
zaktualizowac. Czy chcesz zaktualizowac te systemy plikow?
}

message rootmissing
{ docelowy / jest zagubiony %s.
}

message badroot
{Kompletny nowy system plikow nie przeszedl podstawowych testow.
 Jestes pewien, ze zainstalowales wszystkie wymagane pakiety? 
}

message fddev
{Ktorego urzadzenia dyskietek chcesz uzyc ? }

message fdmount
{Wloz dyskietke zawierajaca plik "%s". }

message fdnotfound
{Nie moglem znalezc pliku "%s" na dysku. Wloz dyskietke
zawierajaca ten plik.}

message fdremount
{Dyskietka nie zostala pomyslnie zamontowana. Mozesz:

Sprobowac jeszcze raz i wlozyc dyskietke z plikiem "%s".

Nie wgrywac wiecej plikow z dyskietek i przerwac proces.
}

message mntnetconfig
{Czy informacje o sieci, ktore podales sa prawidlowe dla tej maszyny
w reguralnej pracy i czy chcesz aby je zapisac w /etc? }

message cur_distsets
{Ponizej jest lista pakietow dystrybucyjnych, ktore zostana uzyte.

}

message cur_distsets_header
{Pakiet dystryb.   Uzyc?
------------------ ----
}

message cur_distsets_row
{%-18s %s\n}

message tarerror
{Pojawil sie blad w trakcie rozpakowywanie pliku %s. To znaczy, ze
pewne pliki nie zostaly prawidlowo rozpakowane i twoj system
nie bedzie kompletny.

Kontynuowac rozpakowywanie pakietow?}

message partitions_overlap
{partycje %c i %c pokrycia.}

message edit_partitions_again
{

Mozesz albo wyedytowac tablice partycji recznie, albo poddac sie
i powrocic do glownego menu.

Edytowac tablice partycji ponownie ?}

message not_regular_file
{Plik konfiguracyjny %s nie jest plikiem regularnym.\n}

message out_of_memory
{Za malo pamieci (alokacja pamieci nie powiodla sie).\n}

message config_open_error
{Nie moglem otworzyc pliku konfiguracyjnego %s\n}

message config_read_error
{Nie moglem odczytac pliku konfiguracyjnego %s\n}

message nobiosgeom
{Sysinst nie mogl automatycznie rozpoznac geometrii dysku z BIOS.
Fizyczna geometria to %d cylindrow %d sektorow %d glowic\n}

message biosguess
{Uzywajac informacji z dysku, najlepsze parametry geometrii dysku z BIOS to
%d cylindrow %d sektorow %d glowic\n}

message cmdfail
{Polecenie
	%s
nie powiodlo sie. Nie moge kontynuowac.}

message aoutfail
{Katalog do ktorego stare a.out wspoldzielone biblioteki powinny byc
przeniesione nie moze zostac utworzony. Sproboj jeszcze raz procedury
aktualizacji i upewnij sie, ze zamontowales wszystkie systemy plikow.}

message noactivepart
{Nie zaznaczyles aktywnej partycji. Moze to spowodowac, ze twoj system nie
uruchomi sie prawidlowo. Czy partycja NetBSD ma zostac zaznaczona jako aktynwa?}

message upgradeparttype
{Jedyna odpowienid partycja, ktora zostala znaleziona dla instalacji NetBSD
jest starego typu NetBSD/386BSD/FreeBSD. Czy chcesz zmienic typ tej partycji
na nowa partycje tylko dla NetBSD?}

message continue
{Kontynuowac?}

message choose_timezone
{Wybierz strefe czasowa, ktora najlepiej ci odpowiada z ponizszej listy.
Nacisnij ENTER aby wybrac. Nacisnij 'x' a potem ENTER aby wyjsc.

 Domyslna:	%s 
 Wybrana:	%s 
 Lokalny czas:	%s %s 
}

message choose_crypt
{Wybierz sposob szyfrowania hasel, ktorego chcesz uzywac. NetBSD moze korzystac
albo z DES albo MD5.

Tradycyjna metoda DES, jest kompatybilna z wiekszoscia unixowych systemow
operacyjnych, ale wtedy tylko 8 pierwszych znakow w hasle jest rozpoznawanych.
Metoda MD5 pozwala na dluzsze hasla, niektorzy twierdza ze jest bardziej
bezpieczna.

Jesli posiadasz siec oraz zamierasz korzystac z NIS, pamietaj o mozliwosciach
innych maszyn w twojej sieci.

Jezeli uaktualniasz swoj system i nie chcesz, aby zostaly dokonane zmiany w
konfiguracji, wybierz ostatnia opcje "nie zmieniaj".
}

message swapactive
{Dysk, ktory wybrales posiada partycje wymiany, ktora moze byc aktualnie
w uzyciu jesli twoj system ma malo pamieci. Poniewaz chcesz zmienic uklad
partycji, partycja wymiany zostanie teraz wylaczona. Moze to spowodowac
pojawienie sie bledow. Jesli zuwazysz takie bledy zrestartuj komputer, 
a nastepnie sprobuj jeszcze raz.}

message swapdelfailed
{Sysinst nie mogl deaktywowac partycji wymiany na dysku, ktory wybrales
do instalacji. Zrestartuj komputer i sprobuj jeszcze raz.}

message rootpw
{Haslo root'a w nowo zainstalowanym systemie nie zostalo jeszcze ustawione,
i dlatego jest puste. Czy chcesz ustawic haslo dla root'a teraz?}

message label_size_special
{
Specjalne wartosci, ktore moga byc podane jako wartosci rozmiaru:
    -1:   az do konca czesci dysku NetBSD
   a-%c:   zakoncz ta partycje tam gdzie partycja X sie zaczyna

}

message label_offset_special
{
Specjalne wartosci, ktore moga byc podane jako wartosci przesuniecia:
    -1:   zacznij na poczatku czesci dysku NetBSD
   a-%c:   zacznij na koncu partycji X 

}

message postuseexisting
{
Nie zapomnij zamontowac wszystkich systemow plikow, ktorych chcesz
uzywac w systemie. Nacisnij <enter> aby kontynuowac.
}

message cur_fs
{Aktualnie wybrane systemy-plikow

}

message cur_fs_header
{   Punkt-montazu  Wybrane\n}

message cur_fs_row
{    %s         %s\n}

message no_root_fs
{
Nie zdefiniowano glownego systemu plikow. Musisz zdefiniowac przynajmniej
jeden punkt montazu z "/".

Nacisnij <enter> aby kontynuowac.
}
