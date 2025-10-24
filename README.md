Ca utilizatori Linux si administratori de sistem, am avut ocazia sa incercam diverse distributii, ca Slackware, SuSE, Gentoo, Redhat, Mandrake si altele. În fiecare am gasit parti bune si parti mai putin bune, precum si unele parti care lipseau. Ne-am format amândoi o parere despre ce ar trebui sa contina o distributie, si când ne-am intâlnit am observat ca parerile noastre coincid. Ca simpli utilizatori ai calculatorului, am fi preferat poate SuSE sau Mandrake, dar ca administratori de sistem, Slackware. Amândoi am optat pentru Slackware, fiind in acelasi timp constienti ca nu vom putea avea diversitatea de pachete din SuSE, dar in alegerea noastra a primat simplitatea si claritatea. Ne-am spus ca ar fi ideal, ca o distributie sa fie si simplu de configurat si intretinut si in acelasi timp sa ofere solutii la indemâna pentru utilizarea de zi cu zi a calculatorului. Cu aceasta idee in minte am inceput sa lucram la Decebal. "Simplitate si putere". Simplitate si pentru utilizatorul neobisnuit cu mediul Linux, simplitate pentru cel obisnuit sau pentru administratorul de sistem, in configurarea lui. La ce ne-am gândit când am spus putere ? La viteza si rigurozitatea cu care se poate configura sistemul.

Dar una dintre cele mai importante aspecte in viziunea noastra, a fost securitatea sistemului. Aproape toate distributiile Linux existente, sufera la acest capitol. Exista si distributii la care securitatea este in prim plan, dar de obicei acestea neglijeaza functionabilitatea, simplitatea sau diversitatea. De la inceput, am vrut sa avem securitatea existenta in sistemele *BSD integrata in Decebal Linux.

Etape parcurse in dezvoltarea proiectului

La inceput, ne-a fost pus la dispozitie de catre Universitatea de Vest Vasile Goldis un laborator, in care am lucrat in luna ianuarie 2004, si apoi am continuat pe calculatoarele noastre, acasa.

La Decebal Linux, nici un pachet precompilat nu a fost luat din alta distributie, totul a fost compilat de la zero. La inceput, ne-am creat un mic sistem de pe care sa putem compila restul pachetelor. Stiam ca vom avea nevoie de un manager de pachete. Cele existente nu ne multumeau din diverse motive. De exemplu, cel din Slackware ni se pare prea primitiv, pe când RPM-ul de la RedHat sau deb-ul de la Debian, prea incarcat de optiuni care oricum nu le-am fi folosit niciodata. Am optat pentru package-managerul de la Archlinux, "pacman" dar in momentul actual avem propriul nostru manager de pachete, la care vom face referire in cele ce urmeaza.

Etapa urmatoare a fost alegerea pachetelor care trebuiau incluse in distributie. De la inceput am vrut sa structuram Decebal in doua parti majore. O parte care sa fie destinata serverelor (furnizor de servicii internet, routere, servere de orice fel...) si a doua, utilizatorilor de acasa. Prioritara a fost prima parte dar in paralel am lucrat si la a doua. Într-un an de zile, nu am folosit acasa nici un alt sistem de operare, closed-source sau open-source. Singurul sistem de operare folosit a fost Decebal Linux si evident apareau probleme care trebuiau rezolvate. Probleme ca : "trebuie sa ard un CD", "trebuie sa ascult muzica", "trebuie sa ma uit la un film", "trebuie sa redactez un document" si altele. Drept urmare, pe langa lucrul la partea de server, am lucrat activ la partea de desktop incluzând astfel pachetele care ne-au fost necesare la un moment dat.

De un foarte mare ajutor ne-au fost discutiile online pe canale ca #rolinux, #linux, #gentoo. Acolo am putut observa care sunt problemele uzuale de care se lovesc utilizatorii, fie ei avansati sau incepatori si sa construim astfel Decebal Linux incât acestia sa fie pe cât posibil scutiti de aceste probleme.

Decebal comparat cu alte distribuitii

De pe Internet se pot descarca sute de distributii. Majoritatea sunt axate pe un singur domeniu (cum ar fi distributiile gen "Rescue", sau cele folosite doar pe post de router...). SuSE si Mandrake sunt destinate in special home-userilor, pe când Slackware si Debian sunt folosite mai mult pe post de server/router. Am incercat sa facem o sinteza a acestora si sa includem in Decebal aceste categorii.

Asemanari cu alte distributii

Fiecare distributie de Linux are particularitatile ei, nu exista doua distributii identice. Aceste particularitati reprezinta de fapt identitatea fiecarei distributii. Acest lucru este bun, puterea open-source vine tocmai din diversitate. Totusi, exista doua standarde care ar trebui respectate, FHS (File Hierarchy System) si LSB (Linux Standard Base). Masura in care sunt respectate aceste standarde ramâne la latitudinea dezvoltatorilor fiecarei distributii. De exemplu, Slackware foloseste BSD-init, pe când Redhat, SuSE si clonele lor folosesc SyS-V init. Pentru Decebal Linux am facut scripturi de initializare proprii, care putem spune ca sunt mai apropiate de BSD-init decât de SyS-V init.

Grupa de pachete "Base" este cam aceeasi la fiecare distributie, la fel cum orice distributie majora include KDE si/sau Gnome. Acestea sunt prezente si in Decebal si in sectiunea 5. sunt prezentate mai pe larg pachetele incluse.

În concluzie, putem spune ca Decebal Linux respecta standardele mai sus amintite in proportie de cca. 90% FHS si 50% LSB.

Imbunatatiri si performante

Securitate

De la inceput am pus un mare accent pe securitate. Nu ne referim la insecuritatea creata de utilizatorul experimentat sau neexperimentat, ci la programele vulnerabile care exista in distributii. În mod normal, când se descopera o astfel de vulnerabilitate, distributiile majore (sau pagina oficiala a programului vulnerabil) pun la dispozitie un "patch" care sa acopere vulnerabilitatea respectiva. Totusi, exista multe programe vulnerabile, a caror vulnerabilitati nu s-au facut publice. Acestea fac ca un sistem sa fie pasibil la atacuri, si sansa ca aceste atacuri sa reuseasca este direct proportionala cu numarul de utilizatori de pe sistem si cu numarul de pachete instalate. Am inclus doar programe care sunt la o versiune stabila, evitând versiunile beta. Unde a fost posibil, am inclus versiuni "hardened", versiuni securizate ale programelor. De exemplu, am pus hardened-php, in loc de php-ul implicit care se gaseste in majoritatea distributiilor. Bineinteles nu putem avea siguranta ca in viitor nu se va descoperi o vulnerabilitate intr-unul dintre programele incluse. Drept urmare, nu ne-am limitat doar la acest tip de protectie, ci am cautat si alte moduri de a preveni tipurile de atac mai des folosite.

SSP (Stack-Smash-Protector)

Peste 90% din programele vulnerabile, sunt vulnerabile din cauza utilizarii defectuase a zonei de memorie numita stiva (in engleza, "stack"). Pentru a preveni acest tip de atac, am compilat toate programele cu protectie impotriva exploatarii de tip "stack smash". Aceasta inseamna ca si daca un program are o eroare de programare, daca el este vulnerabil, un atacator nu se va putea folosi de aceasta vulnerabilitate spre a compromite sistemul. Mai mult, orice program compilat pe Decebal Linux va fi protejat impotriva acestui atac. Si orice tentativa de atac este raportata administratorului de sistem.

forkbomb

Linux este un sistem de operare multi-user si multi-tasking. Adica, exista mai multi utilizatori pe acelasi sistem, care pot fi conectati concomitent. De asemenea, fiecare utilizator poate rula simultan mai multe programe, adica procese. Aproape toate distributiile au o problema majora, care apare din cauza ca sistemul permite fiecarui utilizator un numar nelimitat de procese. Evident acest numar este limitat de resursele hardware, dar daca un utilizator foloseste un program de tip "forkbomb", el poate utiliza in totalitate aceste resurse hardware (memorie si timp de procesor), facând astfel sistemul inutilizabil. Pentru a preveni acest tip de atac, am limitat numarul de procese care pot fi pornite simultan, la 200 pentru fiecare utilizator in parte, si am pus limite pentru timpul de procesor alocat fiecarui utilizator.

kernel patches

Nucleul (in engleza kernel), este sistemul de operare in sine. Tot ce face un sistem de operare, este facut de kernel. De la accesul la dispozitivele hardware, pâna la accesarea memoriei. Deci, pentru a restrictiona accesul la partile sensibile din sistem, este indicat sa se foloseasca un "patch" pentru kernel, patch axat pe securizarea kernel-ului. Decebal vine cu kernel-ul 2.6.10, cu patch-ul grsecurity (http://www.grsecurity.net).

grsecurity

Printre avantajele folosirii grsecurity, putem enumera posibilitatea administratorului de sistem, de a supraveghea tot ce se intâmpla pe sistem. Poate urmari executia programelor (cine, când, ce - lansat in executie), montarea device-urilor si altele. Utilizatorii vor avea acces doar la programele lansate de ei, nu vor vedea tot ce ruleaza pe sistem...

PaX

Scopul proiectului PaX este de a implementa mecanisme de protectie impotriva exploatarii vulnerabilitatilor din programele software. Vulnerabilitati care ofera atacatorilor acces scriere/citire zonei de memorie alocata programelor. PaX ofera protectie contra metodelor de atac, ca : buffer overflow (stack sau heap), format strings si altele. În principiu, PaX face o imagine a zonei de memorie folosita de programe. Stabileste care parti din aceasta zona sunt executabile, si in momentul când se incearca executia unui cod aflat intr-o zona de memorie care in mod normal nu este executabila, opreste executia programului, si implicit atacul.

PaX, este inclus in kernel si poate fi controlat cu programele chpax si paxctl.

O descriere amanuntita a proiectului PaX poate fi gasita pe http://http://pax.grsecurity.net/docs/pax.txt

firewall

Din nici o distributie Linux nu trebuie sa lipseasca un firewall. Drept urmare am inclus in Decebal Linux un firewall, care se remarca prin usurinta in folosire. Cu o simpla comanda se poate bloca accesul la anumite porturi, accesul de la anumite IP-uri, si altele. Utilizatorul nu trebuie sa tina minte sintaxa iptables. Tot din linia de comanda se pot modifica fisierele de configurare a firewallului. Se pot realiza in acest mod, sarcini deosebit de complexe, cu efort minim.

Deocamdata nu are interfata grafica, nici pentru consola nici pentru mediul X dar in viitorul foarte apropiat, vom include si aceasta facilitate.

glibc (GNU C libraries)

Colectia de biblioteci C, glibc, este folosita de toate programele care sunt compilate pentru un sistem Linux. Unele programe sunt vulnerabile din cauza ca nu folosesc corect (sigur) aceste functii din glibc. Drept urmare, am cautat sa securizam aceste biblioteci, glibc-ul. Chiar daca un program este vulnerabil, si un utilizator malitios incearca sa compromita sistemul prin intermediul lui (de fapt a unei functii folosite de program, care se gaseste in glibc), atacul va fi oprit si anuntat administratorului de sistem. Pentru glibc, am inclus patch-urile PaX, propolice-guard-functions, owl-malloc-unlink-sanity-check, dl_execstack-PaX-support, ssp si altele.

daemons

Toti daemonii (servicii) care ruleaza pe Decebal Linux, ruleaza ca useri neprivilegiati (nu ca root). Avantajul este evident. Daca un daemon (apache, ftp...) este compromis, si prin intermediul lui se obtine access la sistem, cel care a intrat in sistem, nu va avea drepturi de root (administrator), ci drepturile utilizatorului care ruleaza acel daemon. Deci, nu va fi tot sistemul compromis, atacatorul nu va avea acces la intreg sistemul, ci doar la fisierele de care se foloseste daemonul in discutie.

dpack

Dupa cum spuneam, Decebal Linux dispune de un manager de pachete propriu. Are interfata grafica si evident poate fi apelat si folosit si din linia de comanda. Organizarea pachetelor se face dupa "grupe" prestabilite ca : "base", "editors"…

Are posiblitatea de a recompila un pachet la alegere, sau o grupa intreaga sau tot sistemul. Acest lucru, ca si la distributia Gentoo, duce la optimizarea Decebal Linux pentru calculatorul pe care este instalat.

De asemenea, dpack, poate sa faca update de pe internet la pachetele instalate care reprezinta un risc de securitate pentru sistem (sau a ultimei versiuni a unui pachet), cu ajutorul programului crontab. De asemenea, fiecare pachet poate fi instalat din sursa, de pe suportul de instalare (CD) sau prin descarcarea de pe internet.

Dependintele fiecarui pachet, le rezolva managerul de pachete, utilizatorul nefiind nevoit sa instaleze manual fiecare dependinta in parte.

sysconf

"sysconf" este o suita de programe destinata configurarii intregului sistem. Interfata este realizata cu meniuri interactive atât pentru consola cât si pentru X, utilizatorul având la dispozitie ajutor contextual pentru majoritatea sectiunilor. Se poate configura reteaua, setarile consolei, mouse-ul, conturile PPP, lista de utilizatori, managerul de pachete si altele. Totul este modular, existând optiunea ca utilizatorul sa ruleze direct un modul, fara a accesa meniul principal. Functiile sale sunt similare cu cele din YaST din distributia SuSE, dar pentru simplitate si claritate, interfata seamana cu cea din scripturile de configurare din distributia Slackware.

Directorul /etc/, init (/etc/rc.d/) si directorul /etc/conf.d

Pe orice sistem Linux, fisierele de configurare se gasesc in directorul /etc. Pe Decebal Linux am cautat sa structuram cât mai clar acest director. Fisierele de initializare sunt mai apropiate de BSD-init decât de SyS-V init, fiecare serviciu având o lista de dependinte si conflicte pe care le rezolva automat la boot-are.

În '/etc/conf.d' am facut mici fisiere de configurare pentru diversi daemoni, setarile consolei si altele.

De exemplu :

# /etc/conf.d/httpd - configuration file for /etc/rc.d/init.d/httpd
# Set this to yes if you want to use SSL.
USE_SSL=yes
# Set this to yes if you want to use MOD_PHP.
USE_PHP=yes
# Set this to yes if you want to use MOD_PERL.
USE_PERL=no
# Set this to yes if you want to use MOD_PYTHON.
USE_PYTHON=no
# Set this to yes if you want to use MOD_TCL.
USE_TCL=no
# Set this to yes if you want to use MOD_MONO.
USE_MONO=no
# Apache additional arguments.
APACHE_OPTS=""

Si al doilea exemplu :

# /etc/conf.d/console - default console settings
# desired EGA/VGA console screen font
FONT=cp857.08.gz
# desired the keyboard translation tables file (keymap)
KEYMAP=us.map
# the keyboard repeat rate and delay time
KEYBOARD_RATE=30
KEYBOARD_DELAY=250
# the interval of inactivity, in minutes, after which the
# screen will be automatically blanked (using APM if available).
BLANK_DELAY=10
# If the kernel supports APM or ACPI power management then you can
# tune what your monitor should do when the BLANK_DELAY is reached
# Available modes:
# on|vsync - Puts the monitor into VESA vsync suspend mode.
# hsync - Puts the monitor into VESA hsync suspend mode.
# powerdown - Puts the monitor into VESA powerdown mode.
# off - urns off monitor VESA powersaving features.
POWERSAVE_MODE=powerdown
# After how much time should the monitor go in powerdown mode (use 0
# to disable it and from 1 to 60 minutes)
POWERDOWN=15
# activate numlock ? (TTYS - the tty's where we will activate numlock)
NUMLOCK=yes
TTYS="1 2 3 4 5 6 7 8 9 10 11"

Pachete incluse

Decebal Linux, printre altele, are KDE 3.3.1, Gnome 2.8, OpenOffice 1.1.3, Firefox 1.0... Lista de pachete este mult prea mare pentru a putea fi inclusa aici, dar poate fi consultata pe pagina http://www.decebal.org. Vom vorbi aici doar de anumite pachete care am considerat ca trebuie mentionate. Le-am grupat in doua categorii, pentru doua tipuri de utilizatori.

Pentru utilizatori obisnuiti cu mediul Windows

-Pachete Office

Ca si solutii Office, am inclus doua suite de pachete, KOffice (din KDE) si OpenOffice.org. Ambele au o interfata aproape identica cu Microsoft Office. Sunt compatibile cu documentele de tip Word, Excel, Powerpoint, OpenOffice având si suport pentru macro-uri, si un interpretor Visual Basic incorporat.

Mai pot fi folosite abiword (clona MS-Word), gnumeric (clona MS-Excel) si gnucash.

-Player audio

Majoritatea utilizatorilor Windows folosesc Winamp pentru a reda fisierele mp3. Player-ul open-source "xmms", are o interfata identica, având egalizator, playlist si oferind tot ce ofera si winamp. El mai vine cu diverse plugin-uri, de output/input, vizuale, si altele. Pentru utilizatorii Windows, din acest punct de vedere, tranzitia se poate face fara nici o dificultate.

-Player video

În mediul Windows, oferta de programe de redare video este foarte vasta. Problema e ca exista foarte multe formate de fisiere video, si programele trebuie sa tina cont de aceste formate ("codec"-uri). Ca solutie la aceasta problema, vine programul open-source MPlayer, care are suport pentru aproape toate codec-urile existente. Interfata este customizabila, in Decebal Linux fiind incluse peste 30 de skin-uri, precum si font-uri cu diacritice.

-Inscriptionare CD/DVD

Utilizatorii obisnuiti cu Nero, nu vor avea nici o dificultate in trecerea la Linux. Programul K3b (www.k3b.org) are o interfata foarte asemanatoare, si este mult mai flexibil.

-Internet

În afara de Konqueror si Galeon din KDE si respectiv Gnome, am inclus navigatoarele Mozilla si Firefox, Epiphany, care reprezinta o alternativa reala la Microsoft Internet Explorer, atât din punct de vedere al usurintei in navigare si consum de resurse dar mai ales din punct de vedere al securitatii. Programele de Instant-Messaging Kopete (www.kde.org) si Gaim (www.gnome.org) ofera utilizatorilor de Windows care au nevoie de Yahoo Messenger, MSN, ICQ sau alte retele o alternativa la Yahoo Messenger, ICQ si altele.

Cei care folosesc Outlook/Outlook Express, nu vor avea nici o dificultate in a folosi Thunderbird (din familia de pachete Mozilla). Este de asemenea mult mai sigur, nefiind vulnerabil la atacurile binecunoscute ale virusilor, viermilor si troianilor care folosesc Outlook/Outlook express spre a se inmulti.

Pentru utilizatorii familiari cu mediul Linux si pentru administratorii de sistem

Pe langa mediul grafic X, am inclus si pentru consola programe care sa nu faca utilizatorul dependent de X, navigatoare grafice, player-e video si audio, clienti Instant Messaging (gaim, centericq, kopete) si IRC (BitchX, irssi, ksirc, xchat)... De asemenea, pentru partea de retele despre care am amintit, am inclus o gama foarte variata de programe. Cu usurinta, Decebal Linux se poate adapta oricarei configuratii de server. Sunt prezenti 4 daemoni de FTP (vsftpd, proftpd, bftpd, tftpd) , apache, postfix cu optiuni de filtrare a mesajelor de tip spam si mesaje infestate cu virusi, imap, pop3. Majoritatea daemonilor pot fi folositi si prin conexiuni sigure, SSL (Secure Socket Layer).

Utilizatorii care folosesc calculatorul acasa, gasesc programe si pentru consola de redare audio (mp3blaster, mpg321), video (mplayer, zgv), inscriptionare CD-uri (cdrtools), navigatoare (lynx, links, elinks)...

Perspective in evolutia Decebal

Viitor apropiat

Dorim sa includem la instalare posibilitatea ca utilizatorii sa poata folosi partitii criptate, si eventual directorul home al fiecarui utilizator sa fie criptat.

Inca nu se poate instala sistemul din surse (compilarea pachetelor in momentul instalarii) dar versiunile ulterioare vor avea aceasta posibilitate. Tot in versiunile ulterioare Decebal va putea fi instalat "de pe internet". Adica, daca un calculator este conectat la internet, programul de instalare va configura conectia la internet (retea sau alt mod), si instalarea pachetelor se va face prin descarcarea lor de pe internet. Acest lucru ar putea duce la micsorarea seminifcativa a kit-ului de instalare (poate fi pe o singura discheta).

De asemenea, lucram la o varianta Decebal Linux, de tip Live-CD, similara cu Knoppix.

Deocamdata, programul de configurare sysconf nu este complet. Lipseste configurarea grafica a firewall-ului

Decebal Linux nu contine destule programe cu caracter didactic, dar ne indreptam atentia catre acest "domeniu". Dorim sa includem programe educationale, sau eventual sa cream o versiune Decebal, dedicata doar studentilor si elevilor (si evident personalului didactic).

Probabil, in luna februarie 2005, vor fi disponibile imaginile a doua cd-uri (pachete precompilate) si unul sau doua cu sursele acestora, pe http://www.decebal.org.

Viitor indepartat

Cand Decebal se va maturiza indeajuns, ne-am gândit ca am putea include pe langa pachetele de baza, si un CD sau câte sunt necesare, cu jocuri pentru Linux. Acest lucru ar incuraja migratia de la Microsoft Windows la mediul Linux.

De asemenea, numarul de pachete dorim sa fie peste 2500, pentru a satisface toate cerintele.

Decebal Linux, poate fi personalizat pentru a fi folosit ca material didactic, stiintific, sau pentru orice alt domeniu. Multitudinea de programe open-source face ca sistemul de operare Linux sa poata fi adaptat oricaror cerinte.

Echipa

- Adrian Teodor Florin - Student la Universitatea de Vest Vasile Goldis Arad, Facultatea de Informatica.

- Calin Miclaus Bogdan - Student la Universitatea de Vest Vasile Goldis Arad, Facultatea de Informatica.
