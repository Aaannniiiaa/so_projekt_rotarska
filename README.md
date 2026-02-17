OPIS PROJEKTU TEMAT 12: AUTOBUS PODMIEJSKI

github: https://github.com/Aaannniiiaa/so_projekt_rotarska.git

O czym jest projekt:
Projekt polega na symulacji autobusu podmiejskiego. Na dworzec przychodzą pasażerowie (w róznym wieku, niektórzy są VIP, niektórzy mają rowery). Następnie kupują bilety w kasie i wchodzą do autobusu. Pojazd ma ograniczoną ilość miejsc dla ludzi i rowerów. Autobus jeździ i wraca po pewnym czasie.

Procesy w projekcie:
W projekcie mamy cztery procesy:

Kasa - sprzedaje bilety, obsługują VIP-ów, rejestruje wchodzące osoby

Dyspozytor - wysyła sygnały sterujące autobusem i pasażerami:
- sygnał 1 - autobus, który stoi mozę odjechać z niepełną liczbą pasażerów - sygnał 2 - pasażerowie nie mogą wsiaść do żadnego autobusu, nie mogą wejść na dworzec

Kierowca - pilnuje wejść w momencie odjazdu, sprawdza limity miejsc i rowerów, odjeżdża co określony czas

Pasażerowie - każdy jest innym procesem, kupuje bilet, przy wejściu do autobusu okazuje bilet, dzieci do lat 8 wchodzą z opiekunem

Zasady działania:
Autobus ma pojemność P osób i R rowerów. Ma dwa wejścia (jedno dla pasażerów z bagażem podręcznym, a drugie dla pasażerów z rowerami), które mieszczą jedną osobę. Odjeżdża co T czasu. W moemncie odjazdu wejścia muszą być puste. Autobus wraca po losowej wartosci Ti czasu i na jego miejsce pojawia się kolejny (maksymalnie N autobusów). Aby wejść do autobusu pasażer ma mieć wcześniej kupiony bilet.

Testy:
Test 1 - testuję czy przy zbyt dużej ilości pasażerów nadmiar ich wejdzie

Test 2 - testuję czy przy zbyt dużej ilości pasażerów z rowerami nadmiar ich wejdzie

Test 3 - testuję czy na sygnał 1 od dyspozytora autobus odjedzie (nawet jeśli jest niepełny)

Test 4 - testuję czy na sygnał 2 nikt nie wejdzie do autobusu

Test 5 - testuję czy autobus odjedzie co czas T

--------------------------------------------------------------------------------------------------------------------------------------------------

1. śRODKOWIDKO I NARZĘDZIA
* System operacyjny - Windows 11 Pro + Debian GNU/Linux 11
* Język - C11
* Kompilator - GCC
* System budowania - Makefile
* Uruchamianie - VS Code

2. BUDOWANIE I URUCHOMIENIE
Budowanie:
make clean
make

Uruchamianie:
./dyspozytor N M INSIDE P R T Ti

N - liczba kierowców
M - liczba pasażerów
INSIDE - limit osób w strefie wejściowej
P - limit miejsc dla pieszych w jednym kursie
R - limit miejsc dla rowerów w jednym kursie
T - okno czasowanie na kompletowanie kursu
Ti - czas powrotu między kursami

Na przykład: ./dyspozytor 2 50 20 10 2 3 2

3. WYMAGANE
* Procesy: symulacja składa się z procesów:
- dyspozytor - uruchamia całość, tworzy IPC i startuje dzieci
- kasa - obsługuje zakup biletu
- driver - symuluje autobusy/kierowców
- pasazer - symuluje pasażerów
* N autobusów i odjazdy cykliczne:
- liczba autobusów = N
- każdy kierowca wykonuje kolejne kursy w pętli
- czas między kursami: Ti
* Pojemność autobusu P i miejsca na rowery R:
- w każdym kursie kierowca pilnuje limitów:
    - P_left - ile miejsc dla pieszych zostalo w kursie
    - R_left - ile miejsc na rowery zostalo w kursie
- do kursu wybierani są pasażerowie z boarding (VIP ma pierwszeństwo), ale tylko jeśli mieszczą się w limitach
* Dwa wejścia do autobusu (osobne dla A i B) + brak wchodzących w chwili odjazdu:
- wejścia są modelowane semaforami:
    -SEM_GATE_A/B - bramka jednoosobowa (tylko jedna osoba naraz przechodzi)
    - SEM_IN_A/B - limit osób przebywających w strefie wejścia
- pasażer wchodzi atomowo: w jednym semop pobiera GATE i odpowiednią liczbę miejsc z IN_A/B (dla dziecka to 2)
- kierowca przed odjazdem bierze SEM_GATE_A/B, dzięki czemu ma gwarancję, że w moemncie odjazdu nikt nie przechodzi przez wejścia
* Pasażer przed wejściem kupuje bilet w kasie:
- normalny pasażer wrzuca żądanie do kolejki ring_kasa w pamięci współdzielonej
- kasa zdejmuje w ring_kasa, a odpowiedź wysyła kolejką komunikatów (msgsnd)
- pasażer czeka blokująco na odpowiedź (msgrcv z mtype=pid)
* VIP ~1%:
- VIP nie stoi w kolejce do kasy: w kodzie ma ścieżkę "omija kasę"
- VIP ma też priorytet przy wyborze do kursu: są dwa ringi boardingu:
    - rb_vip i rb_norm
    - kierowca zawsze próbuje najpierw pobrać z rb_vip, dopiero potem z rb_norm
* Dzieci < 8 lat pod opieką dorosłego - zajmują osobne miejsca:
- pasażer ma losowany wiek 1-80
- jeśli age < 8, to seats = 2 (dziecko + opiekun) i taki pasażer zużywa 2 miejsca z SEM_IN_A/B oraz z limitu P_left
* Autobus po dojeździe wraca po czasie Ti (losowym):
- realizowane w driver jako opóźnienie Ti z użyciem semtimedop (bez sleep i bez bussy-wait)
* Polecenie dyspozytora: sygnał 1 -> autobus może odjechać niepełny:
- dyspozytor po SIGUSR1 przesyła sygnał do kierowców
- kierowca po SIGUSR1 ustawia flagę g_force_depart i przerywa kompletowanie kursu
* Polecenie dyspozytora: sygnał 2 -> pasażerowie nie mogą wsiadać:
- dyspozytor potrafi wykonać soft stop: ustawia flage stop w SHM, wysyła STOP tokeny i kończy IPC
- procesy sprawdzają flagę stop i nie blokują się bez końca

4. STRUKTURA KODU
* src/driver_prog.
    - odczytuje argumenty z linii poleceń
    - podłącza się do pamięci dzielonej i wywołuje driver_main()

* src/driver.c
    - Logika kierowcy/autobusu
    - Pętla kursów
    - Pobiera pasażerów z peronu z priorytetem VIP
    - Wysyła pasażerowi zaproszenie "wsiadasz" przez kolejkę komunikatów msg_invite
    - Przed odjazdem blokuje bramki wejścia
    - Obsługuje sygnały
    - Obsługuje koniec symulacji: flaga stop w SHM, STOP token w ringach

* src/dyspozytor_main.c
    - uruchamia dyspozytor_main(argc, argv)

* src/dyspozytor.c
    - Parsuje paramtery: N, M, INSIDE, P, R, T, Ti
    - Tworzy zasoby IPC:
        - kolejki komunikatów
        - SHM
        - zestaw semaforów
    - Inicjalizuje semafory startowymi wartościami
    - Startuje procesy przez fork+exec: kasa, driver, pasazer
    - Obsługuje sygnały
    - Zamykanie: ustawia flagę stop i launcz_done w SHM, wysyła STOP tokeny do ringow, usuwa IPC i zbiera dzieci

* src/ipc.c
    - Wrappery na semafory i kolejki komunikatów

* src/kasa_prog.c
    - Podłącza się do SHM i odpala kasa_main()

* src/kasa.c
    - Logika kasy
    - Pobiera pasażerów z ring_kasa
    - Po obsłudze odsyła odpowiedź do konktretnego pasażera przez kolejkę komunikatów:
        - msgsnd z mtype = pid pasażera
        - pasażer czeka blokująco na msgrcv
    - Kończy po odebraniu STOP tokenu lub po usunięciu IPC

* src/log.c
    - Wypisywanie do pliku, kolory

* src/pasazer_prog.c
    - Odczytuje argumenty startowe pasażera
    - Podłącza się do SHM i wywołuje passenger_main()

* src/pasazer.c
    - Logika pasażera
    - losuje wiek, wyznacza child, seats
    - wchodzi na strefę wejścia atomowo
    - jeśli normalny: wrzuca się do ring_kasa i czeka na odpowiedz przez msgrcv
    - wrzuca się do peronu do rb_vip lub rb_norm
    - zwiększa token SEM_BOARD_ANY (budzi kierowców)
    - czeka na invite od kierowcy (msgrcv)
    - na koniec oddaje zajęte miejsca w SEM_IN_A/B
    - Reaguje na zakończenie: sprawdza stop w SHM i obsługuje przypadki usuniętego IPC
* src/ring.c
    - Implementacja ring-bufferów w pamięci dzielonej

* src/shm_layout.c
    - Ustawianie wskaznikow na struktury, odlacza SHM

* include/*.h
    - Stałe parametry, rozmiary ringów, definicje struktur, indeksy semaforów

5. OPIS MECHANIKI
* IPC
- Pamięc dzielona - wspólny stan symulacji + bufory cykliczne
- Semafory - synchronizacja bez bussy-waitingu
- Kolejki komunikatów - prywatne odpowiedzi, blokujące

* KOLEJKI
- Do kasy: SHM ring ring_kasa + semafory EMPTY/FULL/MUTEX
- Na boarding: SHM ring rb_vip/norm + semafory + token SEM_BOARD_ANY
- odpowiedzi/zaproszenia: msg (blokujące msgrcv), kierowane po PID

* CZAS
- T - maksymalny czas postoju/zbierania pasażerów w jednym kursie
- Ti - czas powrotu kierowcy między kursami

* STOP
- Dyspozytor kończy symulację przez ustawienie flagi stop w SHM, wrzucenie STOP tokenow do ringow, zeby odblokowac procesy czekjące blokująco

6. RAPORT
Raport z działania zapisywany jest do raport.txt

7. TESTY

Test 1 - testuję czy przy zbyt dużej ilości pasażerów nadmiar ich wejdzie
Tak, obsłużone w : https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/pasazer.c#L187C1-L197C6

Test 2 - testuję czy przy zbyt dużej ilości pasażerów z rowerami nadmiar ich wejdzie
Tak, obsłuzone w: https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/driver.c#L325C9-L337C14

Test 3 - testuję czy na sygnał 1 od dyspozytora autobus odjedzie (nawet jeśli jest niepełny)
Tak, obsluzone w: https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/driver.c#L252C7-L252C49

Test 4 - testuję czy na sygnał 2 nikt nie wejdzie do autobusu
Tak, obsluzone w: https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/dyspozytor.c#L466C7-L480C10

Test 5 - testuję czy autobus odjedzie co czas T
Tak, obsluzone w: https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/driver.c#L253C13-L385C103

8. FUNKCJE WYMAGANE PRZEZ PROJEKT
* Tworzenie i obsługa plików: write, close, read : https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/dyspozytor.c#L272C1-L319C2
* Tworzenie procesów: fork, exec, exit, wait : https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/dyspozytor.c#L283C1-L292C99 , https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/dyspozytor.c#L168C1-L178C1
* Obsługa sygnałów: kill, sigaction : https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/dyspozytor.c#L69C1-L95C2
* Synchronizacja procesów: semget, semctl, semop : https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/dyspozytor.c#L392C3-L392C128 , https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/ipc.c#L19C4-L19C112
* Łącza nazwane i nienazwane: pipe : https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/dyspozytor.c#L275C4-L275C94
* Segmenty pamięci dzielonej: shmget, shmat, shmctl : https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/dyspozytor.c#L370C4-L378C6
* Kolejki komunikatów: msgget, msgctl, msgsnd, msgrcv : https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/dyspozytor.c#L366C4-L366C71 , https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/ipc.c#L85C5-L85C59 , https://github.com/Aaannniiiaa/so_projekt_rotarska/blob/ef4bafe9ee237db103b956221f7130c50997edd2/src/ipc.c#L106C3-L106C120

