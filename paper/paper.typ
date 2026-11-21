#set text(
  font: "Times New Roman",
  size: 12pt,
  lang: "pl",
)

#set par(
  justify: true,
  first-line-indent: (amount: 1cm, all: true),
  leading: 1.5em,
)

#show heading.where(level: 1): it => {
  v(24pt, weak: true)

  set text(
    font: "Times New Roman",
    size: 16pt,
    weight: "bold",
  )

  set par(
    justify: true,
    leading: 1.5em,
  )

  block(width: 100%)[#h(0.76cm)#it]

  v(24pt, weak: true)
}

#show heading.where(level: 2): it => {
  v(18pt, weak: true)
  it
  v(18pt, weak: true)
}

#set heading(numbering: "1.1.")

#set page(numbering: "1 / 1")

#outline(
  title: [Spis treści],
  indent: auto
)

#pagebreak()

= Wstęp

Żyjemy w czasach gdzie smartfon i sieć GSM to nieodłączna część naszego codziennego życia, cała bankowość, ratownictwo, komunikacja, wszystko to opiera się na infrastrukturze telekomunikacyjnej.

Ogromnym problemem jest podatność tej infrastruktury. Pomimo ciągłęgo ulepszania i unowocześniania jej, nadal jest ona dość krucha. Wichury, powodzie, opady śniegu oraz awarie prądu natychmiast sprawiają że stacje GSM stają się bezużyteczne. W przypadku wybuchu konfliktu zbrojnego oczywistym jest że tak krytyczna infrastruktura będzie jednym z pierwszych celów aby upośledzić koordynację kraju w tak kluczowym momencie.

Rośnie również świadomość dotycząca cyberbezpieczeństwa oraz architektury systemów, tego czy nasze dane przechowywane są bezpiecznie, scentralizowane systemy niosą ze sobą ryzyko punktów pojedynczej awarii (Single point of failure), jak i również prywatności, czyli podatność ruchu sieciowego na analizę i profilowanie oraz to, kto i jak przechowuje nasze dane, i komu je udostępnia.

Jak więc rozwiązać te problemy? Praktyczna użyteczność bezinfrastrukturowej komunikacji zależy od tego na ile jest ona bezpieczna, oraz przystępna pod względem finansowym oraz użytkowym, jak i niezależności energetycznej. Tym problemem zajmuje się niniejsza praca, której celem jest implementacja przenośnego oraz niezależnego urządzenia do szyfrowanej komunikacji, które łączy zalety modulacji LoRa z współczesnymi rozwiązaniami kryptografii.

#pagebreak()

= Wprowadzenie

== Cel pracy

Celem niniejszej pracy jest projekt oraz implementacja autonomicznego przenośnego urządzenia (handheld) do bezprzewodowej komunikacji tekstowej na dalekie dystanse, bazującego na technologii LoRa oraz mikrokontrolerze RP2040. Urządzenie umożliwia bezpieczną komunikację za pomocą wiadomości tekstowych, oraz na wymianę plików, cały ruch szyfrowany jest przed emisją radiową, co zapewnia wysoki poziom poufności oraz integralność przesyłanych wiadomości. Urządzenie działa w pełni niezależnie, bez dostępu do publicznej infrastruktury sieciowej (komunikacja off-grid). 

== Zakres

Aby zrealizować ten cel zdefiniowano następujący zakres pracy:
+ Przegląd literatury na temat technologii LoRa, oraz bezpiecznej komunikacji w systemach wbudowanych.
+ Projekt i budowa prototypu urządzenia (mikrokontroler RP2040, moduł radiowy, wyświetlacz LCD, klawiatura)  oraz schemat połączeń.
+ Opracowanie protokołu transmisji oraz komunikacji z mechanizmami szyfrowania End-to-End.
+ Implementacja firmware'u urządzenia, realizującego obsługę modułu LoRa, sterowników peryferiów, interfejsu użytkownika oraz pamięci trwałej.
+ Implementacja oprogramowania na komputer do komunikacji z urządzeniem. 
+ Przeprowadzenie testów działania urządzenia oraz pomiarów zasięgu i jakości sygnału.

== Struktura

Układ pracy jest podporządkowany założonemu zakresowi pracy w następujący sposób:

// DO ZMIANY AI!!!!!!!!!!!!
- *Rozdział 2 (Stan wiedzy)* – zawiera przegląd literatury dotyczącej modulacji LoRa, pasm ISM, standardów
    łączności ad-hoc oraz algorytmów kryptograficznych stosowanych w mikrokontrolerach.
    - *Rozdział 3 (Analiza porównawcza i dobór rozwiązań)* – przedstawia kryteria wyboru komponentów sprzętowych
    (RP2040, SX1262), biblioteki kryptograficznej Monocypher oraz uzasadnienie wyboru architektury peer-to-peer
    zamiast sieci LoRaWAN.
    - *Rozdział 4 (Projekt architektury i protokołu)* – szczegółowo omawia koncepcję urządzenia, format ramek
    radiowych, procedurę parowania (Invite Protocol) oraz architekturę oprogramowania.
    - *Rozdział 5 (Implementacja systemu)* – opisuje szczegóły techniczne firmware'u, implementację maszyn stanów,
    obsługę interfejsu użytkownika oraz warstwy transportowej USB.
    - *Rozdział 6 (Testy i walidacja)* – prezentuje metodykę pomiarową, wyniki testów zasięgu w terenie, pomiary
    narzutu kryptografii oraz weryfikację niezawodności transmisji.
    - *Rozdział 7 (Podsumowanie i wnioski)* – podsumowuje stopień realizacji celów i wskazuje kierunki dalszego
    rozwoju konstrukcji.
// KONIEC AI!!!!

= Stan wiedzy

== Modulacja LoRa i pasma ISM

=== Czym jest modulacja CSS?

// AI NOTE sprawdzic czy te matematyczne na pewno tak sa, i bibliografie do tego

LoRa (Long Range) to technologia transmisji radiowej opracowana przez firmę Semtech, polega ona na modulacji z rozproszonym widmem sygnałem świergotowym (CSS Chirp Spread Spectrum). Główną zasadą działania tego mechanizmu jest wymiana przepustowości na zysk przetwarzania (processing gain) oraz odporność na szum otoczenia (background noise).

\ Przesyłanie danych odbywa się poprzez tak zwane "chirpy" czyli przebieg którego częstotliwość zmienia się w czasie w sposób liniowy i ciągły, przechodząc przez całe pasmo kanału.
\ Przebiegi dzielimy na dwa rodzaje, czyli "up-chirp" oraz "down-chirp", jeśli przebieg zaczyna się na dolnej granicy pasma ($F_c - (B W)/2$) a kończy na górnej ($F_c + (B W)/2$) a nachylenie jest dodatnie ($beta > 0$) to jest to "up-chirp", jeśli natomiast przebieg zaczyna się na górnej granicy a kończy na dolnej z nachyleniem ujemnym ($beta < 0$) to jest to "down-chirp".
\ Dwa rodzaje sygnałow dają tylko dwie wartości które można przesłać, dlatego LoRa wykorzystuje kodowanie M-arne, w którym $M = 2^("SF")$ a pojedyńczy symbol zawiera $"SF"$ bitów danych.
Aby móc reprezentować symbol z zakresu $s in {0, 1, ..., 2^("SF")-1}$ bazowy "up-chirp" podlega cyklicznemu przesunięciu czasowemu (cyclic shift) zależnie od wartości $s$. W efekcie na wykresie częstotliwości względem czasu sygnał zaczyna się od częstotliwości odpowiadającej symbolowi $s$, liniowo narasta do górnej granicy pasma, a następnie "zawija" się spowrotem do dolnej krawędzi pasma i kontynuuje aż do częstotliwości startowej.

\ W pakiecie LoRa wykorzystywane są oba rodzaje przebiegów aby precyzyjnie zsynchronizować czas i częstotliwość. Preambuła pakietu składa się z 8 sygnałów "up-chirp", a następnie z 2 zakodowanych sygnałów "up-chirp" służących do identyfikacji sieci oraz 2,25 nieprzesuniętego sygnału "down-chirp" sygnalizującego początek danych. Po stronie odbiorcy sygnał podlega operacji "de-chirpingu", czyli przemnożeniu odebranego sygnału przez bazowy sygnał o przeciwnym nachyleniu (down-chirp) co przekształca cykliczne przesunięcie czasowe w przebieg o stałej częstotliwości. Następnie stosuje się dyskretną transformatę Fouriera (DFT) która skupia rozproszoną energię przebiegu w odrębnym wąskim prążku widma, co pozwala na detekcję poniżej poziomu szumów.

=== Parametry transmisji

Transmisja LoRa posiada trzy parametry które możemy kontrolować, są to:
- współczynnik rozpraszania (Spreading Factor, SF)
- szerokość pasma (Bandwidth, BW)
- stopa kodowania korekcyjnego (Coding Rate, CR)

\ Czas trwania pojedyńczego przebiegu można policzyć z następującego wzoru:
$ T_"sym"= 2^("SF")/"BW" $

\ Standard LoRa przewiduje współczynnik SF w przedziale od SF7 do SF12 (układ SX1276 dodatkowo wspiera SF6),
co przekłada się na od 128 do 4096 "chipów" na jeden symbol. Podniesienie SF o jeden stopień, powoduje podwojenie długości symbolu oraz czas jego obecności na kanale (Time on Air, ToA), maleje za to przepustowość w zamian za czułość odbiornika i odporność na szum. Natomiast szerokość pasma (BW) wynosi 125 kHz lub 250 kHz (moduł SX1276 obsługuje wartości od 7.8 kHz do 500 kHz). Poszerzenie pasma podwójnie, skraca o połowę czas trwania przebiegu, i zwiększa przepustowość, ale zwiększa ilość szumu w odbiorniku o $10log_10("BW"_2/"BW"_1)$ dB co pogarsza czułość. 

\ Korekcja błędów której używa LoRa to "FEC" (forward error correction), dostępne są 4 stopnie kodowania, od CR 4/5 do CR 4/8, wartość CR 4/5 zapewnia 20% nadmiarowości (1 bit korekcyjny na 4 bity przesyłanych danych),
wartość CR 4/8 zapewnia 100% nadmiarowości co zapewnia naprawę błędów w środowiskach o silnych zakłóceniach. LoRa stosuje układ przeplotu (interleaver) który rozprasza bity wiadomości pomiędzy wieloma słowami kodowymi, w rezultacie nawet najlżejszy i najszybszy kod 4/5 zapewnia istotną odporność na błędy. Całkowity czas trwania pakietu (Time on Air) na podstawie wzoru ze specyfikacji Semtech wynosi
$ T_"air" = (N_"pre" + 4.25) * T_"sym" + [8 + max(ceil((8 * "PL" - 4 * "SF" + 44)/(4("SF" - 2 * "DE")))*("CR"_"idx" + 4), 0)] * T_"sym" $

\ gdzie $N_"pre"$ to liczba sygnałów preambuły, PL to ilość przesyłanych bajtów danych, $"CR"_"idx"$ to liczba $"idx" in {1,2,3,4}$ odpowiadająca odpowiednio dla CR 4/5-4/8, parametr DE jest równy 1 przy włączonej optymalizacji niskich przepustowości (stosuje się ją dla SF11 i SF12 przy BW 125 kHz). Według tego wzoru pakiet o długości 20 bajtów, z podstawowym kodowaniem CR 4/5, BW 125 kHz, oraz SF10, będzie przesyłany przez 0.370688 sekund, lub 370.688 milisekund.

#{
  set par(justify: false)
  set text(size: 9.5pt)
  set par(leading: 0.55em)
  table(
    columns: (auto, auto, auto, auto),
    table.header([*Parametr*], [*Przyjmowane wartości*], [*Wpływ na Time on Air*], [*Wpływ na przesył danych*]),
    [Spreading Factor (SF)], [SF7 - SF12 (SX1276 wspiera SF6)], [Zwiększenie o 1 podwaja $T_"sym"$ oraz ToA], [Większe wartości SF obniżają próg wymaganego SNR aby poprawnie zdekodować wiadomość, lecz bardzo wydłużają ToA i ryzyko kolizji],
    [Bandwidth (BW)], [125 - 250 kHz (SX1276: 7.8 - 500 kHz)], [Odwrotnie proporcjonalny do $T_"sym"$ (Podwojenie BW skraca symbol w pół)], [Szersze pasmo przyspiesza prędkość transmisji, ale wpuszcza do odbiornika więcej szumu tła, obniżając czułość],
    [Coding Rate (CR)], [4/5, 4/6, 4/7, 4/8], [Zwiększa rozmiar pakietu proporcjonalnie do wybranego kodowania], [Wyższe wartości zwiekszają nadmiarowość dla poprawy odporności na błędy, ale zwiększają ToA]
  )
}

=== Czułość odbiornika poniżej progu szumów i duty cycle

LoRa pozwala na dekodowanie sygnałów poniżej progu szumów poprzez skupienie energii rozproszonego sygnału w jednym wąskim prążku podczas "de-chirpingu", teoretyczną czułość odbiornika w dBm wyznacza równanie:
$ S_"RX" = -174 + 10log_10("BW") + "NF" + "SNR"_"limit"("SF") $

\ Gdzie -174 dBm/Hz to gęstość widmowa szumów termicznych w temperaturze pokojowej, BW w hercach, NF to współczynnik szumów odbiornika (dla SX1276 jest to $"NF" = 6 "dB"$), $"SNR"_"limit"$ to minimalny stosunek sygnału do szumu otoczenia który wymagany jest aby poprawnie zdekodować sygnał. Oficjalne dane producenta (Semtech) mówią że próg ten wynosi $-7.5"dB"$ dla $"SF7"$, oraz $"-20 dB"$ dla $"SF12"$.

\ W przypadku standardowej transmisji z parametrami $"BW"= 125 "kHz" (10log_10(125000) approx 50.97 "dB")$ oraz $"NF"= 6 "dB"$ otrzymujemy podłogę szumową (noise floor) wynoszącą około $-117.03 "dBm"$. W rezultacie teoretyczna czułość wynosi około $-124.5 "dBm"$ dla SF7 oraz około $-137.0 "dBm"$ dla SF12. Jeśli dodatkowo zawężymy pasmo i wybierzemy najwyższą wartość SF12, układ SX1276 osiągnie maksymalną czułość katalogową  $-148 "dBm"$. Budżet łącza (Link Budget, jednostka dB) wyznacza równanie:
$ "Link Budget" = P_"TX" + G_"TX" + G_"RX" - L_"loss" - S_"RX" $

gdzie $P_"TX"$ to moc nadajnika, $G_"TX"$ oraz $G_"RX"$ to zyski anten odpowiednio nadajnika i odbiornika, $L_"loss"$ to suma strat kabli i złączy. Maksymalna moc SX1276 wynosi $+20 "dBm" (100 "mW")$ a czułość $-148 "dBm"$, więc teoretyczny budżet łącza sięga $168 "dB"$, a dla standardowych parametrów transmisji (+20 dBm, SF10, BW 125) wynosi $152 "dB"$  (przy zerowych stratach). W realnych warunków jest on jednak niższy ze względu na regulacje prawne określające limity mocy nadawania. 

== Przegląd istniejących rozwiązań

== Kryptografia w systemach wbudowanych

== Istniejące ekosystemy

// AI NOTE!!!!!!!!!!!
      • Meshtastic: Najpopularniejszy projekt open-source. Opisz jak działa (mesh flooding). Wskaż jego wady: konieczność używania smartfona przez Bluetooth/WiFi lub skomplikowanego UI, duży pobór energii
      na ESP32, narzut protokołu.
      • LoRaWAN: Standard przemysłowy. Wyjaśnij, dlaczego nie nadaje się do Twojego celu (wymaga stałych bramek/gatewayów, serwera sieciowego i gwiazdy topologii – brak bezpośredniej łączności P2P między
      dwoma użytkownikami w lesie/górach).
      • Disaster Radio / Reticulum Network Stack: Przykłady alternatywnych stosów ad-hoc.
// AI NOTE!!!!!!!!!!!

= Wybrane algorytmy

== Kryteria doboru stosu technologicznego

== Zastosowane algorytmy kryptograficzne

== Protokół parowania urządzeń

== Protokół wymiany wiadomości oraz ACK

== Algorytm transferu plików

= Implementacja

== Architektura sprzętowa

// AI NOTE!!!!
  2. Platforma sprzętowa: RP2040 vs ESP32 vs STM32:
      • Dlaczego RP2040: 2 rdzenie Cortex-M0+ (133 MHz), elastyczne PIO, brak wbudowanego prądożernego radia Wi-Fi/BT (niższy pobór w stanie czuwania niż ciągle aktywny ESP32), dojrzałe Pico SDK w C++,
      niski koszt.
// AI NOTE!!!!!!!!

== Środowisko programistyczne 

== Opis maszyn stanów oraz przepływu danych

== Interfejsy użytkownika

= Przyklady eksperymenty testowanie

= Wnioski

== 

// AI NOTE!!!!!!!!!
  • Samokrytyka i ograniczenia rozwiązania (bardzo cenione na obronie):
      • Brak sprzętowego bezpiecznego magazynu kluczy (RP2040 w wersji A nie posiada wbudowanego bezpiecznego elementu CryptoAuth ani sprzętowego szyfrowania pamięci Flash – klucze są chronione programowo).
      • Ograniczona przepustowość LoRa: przesyłanie plików jest wolne i podatne na zakłócenia przy dużych rozmiarach danych.
      • Regulacje prawne pasma 433 MHz (konieczność respektowania ograniczeń czasu nadawania - duty cycle).
  • Wnioski z realizacji:
      • Potwierdzono możliwość stworzenia w pełni funkcjonalnego, bezpiecznego komunikatora off-grid na podzespołach o łącznym koszcie poniżej 150 zł.
      • Wykorzystanie biblioteki Monocypher zapewniło czas szyfrowania i deszyfrowania rzędu pojedynczych milisekund, co jest pomijalne w stosunku do czasu transmisji radiowej w kanale LoRa (Time-on-Air).
  • Kierunki dalszego rozwoju:
      • Migracja na mikrokontroler RP2350 (rdzeń Cortex-M33, obsługa ARM TrustZone, sprzętowe akceleratory SHA-256 i bezpieczny rozruch Secure Boot).
      • Dodanie kompresji danych (np. algorytm heatshrink lub miniz) przed szyfrowaniem w celu skrócenia czasu Time-on-Air przy transferze tekstu i plików.
      • Opcjonalny routing wieloskokowy (mesh) dla małej liczby węzłów.
// AI NOTE!!!!!!!!!!!!

// AI NOTE!!!
  Wskazówki do Bibliografii (co cytować):

  1. Semtech Corporation – Dokumentacja układu SX1261/SX1262 (SX1261/2 Data Sheet).
  2. Raspberry Pi Ltd. – RP2040 Datasheet oraz Raspberry Pi Pico C/C++ SDK.
  3. L. Louédic (Monocypher) – Oficjalna dokumentacja i specyfikacja biblioteki kryptograficznej Monocypher.
  4. RFC 7748 – Elliptic Curves for Security (Krzywe Curve25519 / X25519).
  5. RFC 8439 – ChaCha20 and Poly1305 for IETF Protocols.
  6. Artykuły naukowe z IEEE Xplore dotyczące analizy zasięgu modulacji LoRa (hasła: LoRa path loss models, LoRa CSS performance under noise).
  7. Normy ETSI dotyczące wykorzystania pasm nielicencjonowanych ISM w Europie (Short Range Devices - SRD).
// AI NOTE!!!