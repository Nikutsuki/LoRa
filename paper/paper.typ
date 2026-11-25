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
  indent: auto,
)

#pagebreak()

= Wstęp

Żyjemy w czasach gdzie smartfon i sieć GSM to nieodłączna część naszego codziennego życia, cała bankowość, ratownictwo, komunikacja, wszystko to opiera się na infrastrukturze telekomunikacyjnej.

Ogromnym problemem jest podatność tej infrastruktury. Pomimo ciągłęgo ulepszania i unowocześniania jej, nadal jest ona dość krucha. Wichury, powodzie, opady śniegu oraz awarie prądu natychmiast sprawiają że stacje GSM stają się bezużyteczne. W przypadku wybuchu konfliktu zbrojnego oczywistym jest że tak krytyczna infrastruktura będzie jednym z pierwszych celów, aby upośledzić koordynację kraju w tak kluczowym momencie.

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

LoRa (Long Range) to technologia transmisji radiowej opracowana przez firmę Semtech, polega ona na modulacji z rozproszonym widmem sygnałem świergotowym (CSS Chirp Spread Spectrum) @semtech_an1200. Główną zasadą działania tego mechanizmu jest wymiana przepustowości na zysk przetwarzania (processing gain) oraz odporność na szum otoczenia (background noise).

\ Przesyłanie danych odbywa się poprzez tak zwane "chirpy" czyli przebieg którego częstotliwość zmienia się w czasie w sposób liniowy i ciągły, przechodząc przez całe pasmo kanału @semtech_an1200.
\ Przebiegi dzielimy na dwa rodzaje, czyli "up-chirp" oraz "down-chirp", jeśli przebieg zaczyna się na dolnej granicy pasma ($F_c - (B W)/2$) a kończy na górnej ($F_c + (B W)/2$) a nachylenie jest dodatnie ($beta > 0$) to jest to "up-chirp", jeśli natomiast przebieg zaczyna się na górnej granicy a kończy na dolnej z nachyleniem ujemnym ($beta < 0$) to jest to "down-chirp".
\ Dwa rodzaje sygnałów dają tylko dwie wartości które można przesłać, dlatego LoRa wykorzystuje kodowanie M-arne, w którym $M = 2^("SF")$ a pojedynczy symbol zawiera $"SF"$ bitów danych @semtech_an1200.
Aby móc reprezentować symbol z zakresu $s in {0, 1, ..., 2^("SF")-1}$ bazowy "up-chirp" podlega cyklicznemu przesunięciu czasowemu (cyclic shift) zależnie od wartości $s$. W efekcie na wykresie częstotliwości względem czasu sygnał zaczyna się od częstotliwości odpowiadającej symbolowi $s$, liniowo narasta do górnej granicy pasma, a następnie "zawija" się z powrotem do dolnej krawędzi pasma i kontynuuje aż do częstotliwości startowej.

\ W pakiecie LoRa wykorzystywane są oba rodzaje przebiegów, aby precyzyjnie zsynchronizować czas i częstotliwość. Preambuła pakietu składa się z 8 sygnałów "up-chirp" (jest ona natomiast konfigurowalna, bazowo 8, w SX1276 minimum 6), a następnie z 2 zakodowanych sygnałów "up-chirp" _*Sync word*_ służących do identyfikacji sieci oraz 2,25 nieprzesuniętego sygnału "down-chirp" sygnalizującego początek danych (_SFD, Start frame delimiter_) @sx1276_datasheet. Po stronie odbiorcy sygnał podlega operacji "de-chirpingu", czyli przemnożeniu odebranego sygnału przez bazowy sygnał o przeciwnym nachyleniu (down-chirp) co przekształca cykliczne przesunięcie czasowe w przebieg o stałej częstotliwości. Następnie stosuje się dyskretną transformatę Fouriera (DFT) która skupia rozproszoną energię przebiegu w odrębnym wąskim prążku widma, co pozwala na detekcję poniżej poziomu szumów @semtech_an1200.

=== Parametry transmisji

Transmisja LoRa posiada trzy parametry które możemy kontrolować, są to:
- współczynnik rozpraszania (Spreading Factor, SF)
- szerokość pasma (Bandwidth, BW)
- stopa kodowania korekcyjnego (Coding Rate, CR)

\ Czas trwania pojedynczego przebiegu można policzyć z następującego wzoru @semtech_an1200[s. 10]:
$ T_"sym"= 2^("SF")/"BW" $

\ Standard LoRa przewiduje współczynnik SF w przedziale od SF7 do SF12 (układ SX1276 dodatkowo wspiera SF6),
co przekłada się na od 128 do 4096 "chipów" na jeden symbol @sx1276_datasheet[s. 27]. Podniesienie SF o jeden stopień, powoduje podwojenie długości symbolu oraz czas jego obecności na kanale (Time on Air, ToA), maleje za to przepustowość w zamian za czułość odbiornika i odporność na szum. Natomiast szerokość pasma (BW) wynosi 125 kHz lub 250 kHz (moduł SX1276 obsługuje wartości od 7.8 kHz do 500 kHz). Poszerzenie pasma podwójnie, skraca o połowę czas trwania przebiegu, i zwiększa przepustowość, ale zwiększa ilość szumu w odbiorniku o $10log_10("BW"_2/"BW"_1)$ dB co pogarsza czułość @semtech_an1200.

\ Korekcja błędów której używa LoRa to "FEC" (forward error correction), dostępne są 4 stopnie kodowania, od CR 4/5 do CR 4/8, wartość CR 4/5 zapewnia 25% nadmiarowości (1 bit korekcyjny na 4 bity przesyłanych danych),
wartość CR 4/8 zapewnia 100% nadmiarowości co zapewnia naprawę błędów w środowiskach o silnych zakłóceniach. LoRa stosuje układ przeplotu (interleaver), który rozprasza bity wiadomości pomiędzy wieloma słowami kodowymi, w rezultacie nawet najlżejszy i najszybszy kod 4/5 zapewnia istotną odporność na błędy @semtech_an1200. Całkowity czas trwania pakietu (Time on Air) na podstawie wzoru ze specyfikacji Semtech wynosi @semtech_an1200[s. 10] @sx1276_datasheet[sekcja 4.1.1.6--7, s. 29--31]
$
  T_"air" = (N_"pre" + 4.25) * T_"sym" + [8 + max(ceil((8 * "PL" - 4 * "SF" + 44)/(4("SF" - 2 * "DE")))*("CR"_"idx" + 4), 0)] * T_"sym"
$

\ gdzie $N_"pre"$ to liczba sygnałów preambuły, PL to ilość przesyłanych bajtów danych, $"CR"_"idx"$ to liczba $"idx" in {1,2,3,4}$ odpowiadająca odpowiednio dla CR 4/5-4/8, parametr DE jest równy 1 przy włączonej optymalizacji niskich przepustowości (stosuje się ją dla SF11 i SF12 przy BW 125 kHz), stała 44 odpowiada włączonemu nagłówkowi ($"IH" = 0$) i włączonej sumie kontrolnej ładunku ($"CRC" = 1$). Według tego wzoru pakiet o długości 20 bajtów, z podstawowym kodowaniem CR 4/5, BW 125 kHz, oraz SF10, będzie przesyłany przez 0.370688 sekund, lub 370.688 milisekund.

#figure({
    set par(justify: false)
    set text(size: 9.5pt)
    set par(leading: 0.55em)
    table(
      columns: (auto, auto, auto, auto),
      table.header([*Parametr*], [*Przyjmowane wartości*], [*Wpływ na Time on Air*], [*Wpływ na przesył danych*]),
      [Spreading Factor (SF)],
      [SF7 - SF12 (SX1276 wspiera SF6)],
      [Zwiększenie o 1 podwaja $T_"sym"$ oraz ToA],
      [Większe wartości SF obniżają próg wymaganego SNR, aby poprawnie zdekodować wiadomość, lecz bardzo wydłużają ToA i ryzyko kolizji],

      [Bandwidth (BW)],
      [125 - 250 kHz (SX1276: 7.8 - 500 kHz)],
      [Odwrotnie proporcjonalny do $T_"sym"$ (Podwojenie BW skraca symbol w pół)],
      [Szersze pasmo przyspiesza prędkość transmisji, ale wpuszcza do odbiornika więcej szumu tła, obniżając czułość],

      [Coding Rate (CR)],
      [4/5, 4/6, 4/7, 4/8],
      [Zwiększa rozmiar pakietu proporcjonalnie do wybranego kodowania],
      [Wyższe wartości zwiekszają nadmiarowość dla poprawy odporności na błędy, ale zwiększają ToA],
    )
  },
  caption: [Parametry modulacji LoRa i ich efekt na transmisję]
)

=== Czułość odbiornika poniżej progu szumów i duty cycle

LoRa pozwala na dekodowanie sygnałów poniżej progu szumów poprzez skupienie energii rozproszonego sygnału w jednym wąskim prążku podczas "de-chirpingu", teoretyczną czułość odbiornika w dBm wyznacza równanie @semtech_an1200[s. 13]:
$ S_"RX" = -174 + 10log_10("BW") + "NF" + "SNR"_"limit"("SF") $

\ Gdzie -174 dBm/Hz to gęstość widmowa szumów termicznych w temperaturze pokojowej, BW w hercach, NF to współczynnik szumów odbiornika (dla SX1276 jest to $"NF" = 6 "dB"$), $"SNR"_"limit"$ to minimalny stosunek sygnału do szumu otoczenia, który wymagany jest, aby poprawnie zdekodować sygnał. Oficjalne dane producenta (Semtech) mówią że próg ten wynosi $-7.5"dB"$ dla $"SF7"$, oraz $"-20 dB"$ dla $"SF12"$ @sx1276_datasheet[Tabela 13, s. 27].

\ W przypadku standardowej transmisji z parametrami $"BW"= 125 "kHz" (10log_10(125000) approx 50.97 "dB")$ oraz $"NF"= 6 "dB"$ otrzymujemy podłogę szumową (noise floor) wynoszącą około $-117.03 "dBm"$. W rezultacie teoretyczna czułość wynosi około $-124.5 "dBm"$ dla SF7 oraz około $-137.0 "dBm"$ dla SF12. Jeśli dodatkowo zawężymy pasmo i wybierzemy najwyższą wartość SF12, układ SX1276 osiągnie maksymalną czułość katalogową  $-148 "dBm"$ @sx1276_datasheet[s. 1]. Budżet łącza (Link Budget, jednostka dB) wyznacza równanie @semtech_an1200[s. 15--16]:
$ "Link Budget" = P_"TX" + G_"TX" + G_"RX" - L_"loss" - S_"RX" $

\ gdzie $P_"TX"$ to moc nadajnika, $G_"TX"$ oraz $G_"RX"$ to zyski anten odpowiednio nadajnika i odbiornika, $L_"loss"$ to suma strat kabli i złączy. Maksymalna moc SX1276 wynosi $+20 "dBm" (100 "mW")$ a czułość $-148 "dBm"$, więc teoretyczny budżet łącza sięga $168 "dB"$, a dla standardowych parametrów transmisji (+20 dBm, SF10, BW 125) wynosi $152 "dB"$  (przy zerowych stratach). W realnych warunkach jest on jednak niższy ze względu na regulacje prawne określające limity mocy nadawania.

=== Regulacje prawne w Europie: pasma EU433 i EU868

Zasady użytkowania pasm ISM (_Industrial, Scientific, Medical_) i SRD (ang. _Short Range Devices_) w Europie reguluje norma ETSI EN 300 220 @etsi_300_220_2 zalecenie CEPT ERC/REC 70-03. W Polsce tymi normami zajmuje się Ministerstwo Cyfryzacji, poprzez "rozporządzenie Ministra Cyfryzacji w sprawie rozszerzonego zakresu urządzeń radiowych nadawczych lub nadawczo-odbiorczych, które mogą być używane bez pozwolenia radiowego. Na podstawie art. 145 ust. 4 ustawy z dnia 12 lipca 2024 r." @bip_uke.

\ Pasmo EU433 obejmuje zakres częstotliwości 433.050-434.790 MHz, a pasmo EU868 obejmuje 863-870 MHz, różnią się one maksymalną dopuszczalną mocą transmisji, oraz regułami czasu dostępu do widma (cykl pracy i techniki LBT+AFA, Listen Before Talk + Adaptive Frequency Agility, nazwane w normie "polite spectrum access") @etsi_300_220_2[Table B.1, s. 21--22].

#figure({
  set par(justify: false)
  set text(size: 9.5pt)
  set par(leading: 0.55em)
  table(
    columns: (auto, auto, auto, auto),
    table.header([*Pasmo / Podpasmo*], [*Zakres częstotliwości*], [*Maksymalna moc*], [*Limit duty cycle*]),
    [EU433 - Pasmo H], [433.050-434.790 MHz], [10 mW (+10 dBm)], [$<=10% "duty cycle"$],
    [EU433 - Pasmo I], [433.050-434.790 MHz], [1 mW (0 dBm)], [Brak wymogu],
    [EU433 - Pasmo J], [434.040-434.790 MHz], [10 mW (+10 dBm)], [Brak wymogu],
    [EU868 - Pasmo K], [863.000-865.000 MHz], [25 mW (+14 dBm)], [$<=0.1% "duty cycle lub PSA"$],
    [EU868 - Pasmo L], [865.000-868.000 MHz], [25 mW (+14 dBm)], [$<=1% "duty cycle lub PSA"$],
    [EU868 - Pasmo M], [868.000-868.600 MHz], [25 mW (+14 dBm)], [$<=1% "duty cycle lub PSA"$],
    [EU868 - Pasmo N], [868.700-869.200 MHz], [25 mW (+14 dBm)], [$<=0.1% "duty cycle lub PSA"$],
    [EU868 - Pasmo O], [869.400-869.650 MHz], [500 mW (+27 dBm)], [$<=10% "duty cycle lub PSA"$],
  )
  },
  caption: [Zestawienie parametrów i pasm EU433 oraz EU868 bazująca na normie ETSI EN 300 220-2 @etsi_300_220_2[Tabela B.1].]
)

\ W rezultacie normy te wymuszają wprowadzenie programowego ograniczenia mocy nadajnikowo-odbiorników (SX1276 jest w stanie nadawać z mocą +20 dBm) do +10 dBm w przypadku EU433, lub +14 dBm dla EU868, dużym problemem jest również limit _Duty Cycle_, który w przypadku 1% daje urządzeniu tylko 36 sekund aktywnej transmisji na godzinę. W przypadku standardowych parametrów transmisji z podstawowym kodowaniem CR 4/5, BW 125 kHz, oraz SF10, a więc ToA rzędu 371 milisekund (dla 20 bajtów danych) otrzymujemy możliwość wysłania pojedynczego pakietu około raz na 37.1 sekund ($approx 97$ pakietów na godzinę), w przypadku wykorzystania techniki "meshowania", która wymaga retransmitowania ruchu innych użytkowników, to właśnie _Duty Cycle_ stanowi największe wąskie gardło przepustowości.

== Przegląd istniejących rozwiązań łączności P2P

=== LoRaWAN 

LoRaWAN to otwarty (open-source) protokół operujący w warstwie MAC i warstwie sieciowej zarządzanym przez LoRa Alliance, działa on bezpośrednio nad fizyczną warstwą LoRa @lorawan_energy_tutorial[s. 5]. Architektura LoRaWAN bazuje na topologii *gwiazdy gwiazd* (_stars-of-stars_), w której urządzenia końcowe (_End Devices_, ED) wysyłają pakiety tylko w komunikacji jednoskokowej (_single-hop_) za pomocą niesynchronizowanego protokołu ALOHA do bramek (_Gateways_, GW). Bramki pełnią tylko rolę mostów przekazujących pakiety LoRa poprzez łącze IP (poprzez protokół MQTT) do infrastruktury serwerowej @lorawan_energy_tutorial[s. 5]. 

\ Siecią LoRaWAN zarządzają trzy komponenty serwerowe @lorawan_energy_tutorial[s. 10--11]:
- *Network Server (LNS/NS)* odpowiedzialny jest za deduplikację tych samych pakietów które zostały odebrane przez różne bramki, jak i zarządzanie warstwą MAC.
- *Join Server (JS)* zajmuje się przechowywaniem kluczy głównych (_NwkKey_, _AppKey_), oraz obsługuje procedurę uwierzytelniania i generuje klucze sesji.
- *Application Server (AS)* zajmuje się odbieraniem i deszyfrowaniem danych aplikacji.

\ Zastosowanie LoRaWAN w celu autonomicznej komunikacji _off-grid_ uniemożliwia między innymi @lorawan_energy_tutorial:
- Brak wieloskokowego routingu pomiędzy węzłami końcowymi, w efekcie utrata zasięgu do bramki sprawia że urządzenie zostaje sparaliżowane, nowsze rozszerzenia protokołu wprowadzają "węzły przekaźnikowe (_relay_)" ograniczone do maksymalnie 16 urządzeń na przekaźnik i wyłącznie przekazywanie pakietów w stronę bramki @lorawan_hybrid_2025[s. 3].
- LoRaWAN wymaga kosztownych bramek i sprawnego, stałego łącza do serwerów LNS i JS, Join Server stanowi aktualnie pojedynczy punkt awarii (_single point of failure_) jeśli chodzi o uwierzytelnianie i bezpieczeństwo.
- Klasy pracy protokołu (Klasa A, B, C) zostały zaprojektowane z myślą o wysyłaniu asymetrycznej telemetrii do chmury, energooszczędna Klasa A posiada krótkie okno odbiorcze wyłącznie po własnej transmisji, co uniemożliwia asynchroniczną wymianę wiadomości _peer-to-peer_, natomiast ciągłe nasłuchiwanie w Klasie C, jest bardzo energochłonne i zmniejsza czas życia urządzenia na baterii @lorawan_energy_tutorial[s. 10--11].

=== Meshtastic i routing mesh flooding

Meshtastic jest otwartym projektem typu _mesh_, który wykorzystuje technologię LoRa w nielicencjonowanych pasmach ISM w celu komunikacji tekstowej, udostępniania pozycji GPS, lub telemetrii bez użycia infrastruktury GSM @meshtastic_docs. Meshtastic nie korzysta z bramek lub serwerów centralnych, protokół implementuje algorytm *managed flooding*, polega on na kontrolowanym zalewaniu sieci z jednoczesnym tłumieniem retransmisji. Dzięki temu urządzenia nie muszą przechowywać tablic routingu ani topologii, każdy pakiet posiada parametr _hop limit_, który jest zmniejszany o jeden po każdym przekazaniu aż osiągnie wartość 0, urządzenia opóźniają transmisję pakietów, i rezygnują z niej całkowicie jeśli odbiorą że dany pakiet został retransmitowany.

\ Największą zaletą Meshtastic jest gotowość do pracy od razu po włączeniu urządzenia, bez konfiguracji, oraz jedna z największych społeczności wśród otwartych sieci LoRa, co czyni go najlepszym i najłatwiej dostępnym kandydatem do szybkiego wdrożenia w sytuacji kryzysowej, w obszarze pozbawionym tradycyjnej infrastruktury telekomunikacyjnej. Nie jest on jednak pozbawiony problemów, najważniejsze z nich to:
- Ograniczona skalowalność i zatory, LoRa posiada przepustowość rzędu kilkuset bitów na sekundę, przez to że kanał jest współdzielony w przypadku większych lub gęstych sieci mechanizm _floodingu_ powoduje kolizje, gubienie pakietów (_packet loss_) oraz opóźnienia @croce_2018[s. 1--3].
- Restrykcje Duty Cycle powodują że pomimo wolnego kanału, urządzenia są zmuszone przestać nadawać, przez co nie mogą retransmitować pakietów, i następuje fragmentacja topologii sieci.
- Limit liczby przeskoków oraz nieoptymalne przesyłanie pakietów, Meshtastic nie optymalizuje kierunku przesyłu pakietów i po prostu przesyła każdy pakiet dalej, co w rezultacie skutkuje przesyłaniem pakietów w kierunku przeciwnym do odbiorcy docelowego, jak i ograniczony zasięg komunikacji.

=== Reticulum Network Stack i Disaster Radio

*Reticulum Network Stack (RNS)* to zaprojektowany przez Marka Qvista stos sieciowy skupiający się na kryptografii, działa on w przestrzeni użytkownika (_userland_) i nie wykorzystuje protokołu IP (istnieje jednak możliwość tunelowania ruchu przez TCP/UDP) @reticulum_manual. Kluczową zaletą RNS jest to że działa na dużej ilości typów łącz. Jeden system automatycznie bez dodatkowej konfiguracji łączy moduły radiowe (LoRa/RNode, VHF/UHF, TNC/KISS), siecią IP poprzez Wi-Fi lub Ethernet, portami szeregowymi, stdio czy I2P, a jednocześnie automatycznie zarządza ruchem i daje priorytet dla komunikacji lokalnej.

\ Reticulum zamiast prostego floodingu implementuje wieloskokowe trasowanie z wydzielaniem węzłów transportowych (_transport nodes_), które pełnią funkcję rozproszonego magazynu kluczy i ścieżek @reticulum_manual, ważne jest że nie każdy węzeł w sieci jest węzłem transportowym, ponieważ prowadzi to do degradacji wydajności. Adresowanie działa na podstawie zdecentralizowanych skrótach kluczy publicznych (destinations) a pakiety nie posiadają adresów nadawcy, co zapewnia anonimowość inicjatora. Cały ruch zabezpiecza szyfrowanie _end-to-end_ przy użyciu tymczasowych kluczy X25519. Aby nawiązać w pełni szyfrowaną sesję wymagane są zaledwie 3 pakiety, o łącznym rozmiarze 297 bajtów, a utrzymanie jej potrzebuje tylko 0.44 bps @reticulum_manual. RNS wykorzystuje między innymi:
- Nomad Network
- Protokół LXMF

\ Należy pamiętać że autor sam ostrzega że stos jest młody, i nie przeszedł jeszcze żadnego zewnętrznego audytu bezpieczeństwa.

\ *Disaster Radio* jest projektem który skupia się na budowie tanich, zasilanych solarnie, węzłów łączności kryzysowej, głównie na mikrokontrolerach ESP32 z modułem LoRa @disaster_radio_wiki. Największą cechą wyróżniającą projekt jest możliwość korzystania z aplikacji bez instalacji klienta, urządzenie rozgłasza sieć Wi-Fi w której pod adresem 192.168.4.1 udostępnia interfejs poprzez serwer HTTP oraz WebSockets (alternatywą jest aplikacja Android i łączność poprzez Bluetooth). Projekt korzysta z autorskiego protokołu na bazie LoRaLayer2, każdy węzeł posiada swój unikalny 4 bajtowy adres, oraz dla każdego odebranego pakietu podejmuje decyzję o retransmisji. Projekt pozwala również na zapisywanie historii wiadomości na karcie SD @disaster_radio_wiki. 

=== Porównanie istniejących technologii

#figure({
  set par(justify: false)
  set text(size: 9.5pt)
  set par(leading: 0.55em)
  table(
    columns: (auto, auto, auto, auto),
    table.header([*Technologia*], [*Architektura*], [*Medium i kryptografia*], [*Ograniczenia*]),
    [LoRaWAN], [*Gwiazda gwiazd*, wymagana centralna infrastruktura], [Tylko LoRa, AES-128], [Brak możliwości pracy off-grid],
    [Meshtastic], [*Kratowa (_mesh_)* brak infrastruktury centralnej], [Tylko LoRa, Szyfrowanie kanałowe], [Kolizje oraz wysoki pobór mocy],
    [Disaster Radio], [*Kratowa (_mesh_)* brak infrastruktury centralnej], [LoRa + Wi-Fi / BLE, brak szyfrowania E2EE], [Ograniczona funkcjonalność],
    [Reticulum], [*Multi-hop*, wydzielone węzły transportowe, brak centralnej infrastruktury], [LoRa (RNode), Ethernet, Wi-Fi, I2P, KISS, TCP/UDP, E2EE (X25519, Ed25519, AES-256-CBC + HMAC-SHA256)], [Brak zewnętrznego audytu bezpieczeństwa]
  )
  },
  caption: [Porównanie przykładowych systemów komunikacji bazujących na technologii LoRa]
)

#pagebreak()

== Kryptografia w systemach wbudowanych

=== Wyzwania braku akceleracji sprzętowej mikrokontrolerów ARM Cortex

Implementacja nowoczesnych algorytmów kryptografii na prostych, tanich, oraz energooszczędnych mikrokontrolerach ARM Cortex-M0 oraz ARM Cortex-M0+ szybko napotyka problemy poprzez ograniczenia sprzętowe @dull_2015 @arissen_2015. Mikrokontroler Cortex-M0 posiada tylko 16 rejestrów 32-bitowych (R0-R15), trzy górne są zarezerwowane przez system i pełnią funkcje:
- R13 - wskaźnik stosu
- R14 - rejestr powrotu
- R15 - licznik programu

\ Pozostawia to 13 rejestrów dla programu, a w przypadku trybu "Thumb", instrukcje arytmetyczne mają dostęp tylko do rejestrów R0-R7 (rejestry R8-R12 dostępne są tylko dla niektórych instrukcji takich jak MOV, CMP, ADD, przez co często wymagany jest tak zwany _register spilling_), w celu przetwarzania 256-bitowych liczb. Drugim problemem są ograniczenia pamięciowe, popularny układ STM32F051 posiada tylko 8 KB pamięci RAM i 64 KB pamięci Flash.

\ Na wydajność obliczeniową tych mikrokontrolerów wpływa między innymi @arissen_2015[s. 5--9]:
- Zastosowanie architektury von Neumanna, która współdzieli pojedynczą magistralę zarówno dla instrukcji jak i danych, co sprawia ze operacje odczytu oraz zapisu pamięci (LDR / STR, jak i LDM / STM które wymagają n + 1 cykli procesora) wprowadzają opóźnienia, i sprawiają że procesor musi czekać, aby pobrać następną instrukcję programu, w rezultacie ilość wykonywanych instrukcji na sekundę (MIPS) maleje. 
- Brak instrukcji DSP, układów wektorowych SIMD, oraz mnożenia rozszerzającego UMULL, dostępne są one dopiero w droższych mikrokontrolerach Cortex-M4 @cortex_m4_curve25519.
- Dostępne jest jedynie mnożenie 32-bitowe MULS, w zależności od tego jak jest zaimplementowane wykonuje się w 1 cykl procesora, lub aż w 32 cykle procesora w układach zoptymalizowanych pod minimalną powierzchnię układu @dull_2015[s. 8].
- Brak koprocesorów AES oraz jednostek mnożenia bezprzeniesieniowego (carry-less multiplication) wykorzystywanych w celu uwierzytelniania GHASH @aes_gcm_iot_2019.

\ Dużym problemem jest również podatność na tak zwane _side channel attacks_. Czas wykonywania instrukcji może ulegać fluktuacjom z powodu konfliktu na magistrali podczas operacji DMA, oczekiwania na pamięć lub tego która gałąź programu została wybrana na podstawie tajnych bitów @bernstein_cache_2005. Każdy algorytm kryptograficzny powinien być realizowany w sposób stałoczasowy (_constant-time_), bez instrukcji warunkowych które powodują rozgałęzienie programu i tablic indeksowanych tajnymi danymi.

=== RSA vs ECC oraz Curve25519

W systemach wbudowanych oraz sieciach LoRa klasyczna kryptografia RSA została zastąpiona przez kryptografię krzywych eliptycznych (ECC), ECC osiąga poziom bezpieczeństwa podobny do tradycyjnego RSA, przy znacząco krótszych kluczach (128-bitowe klucze AES, odpowiadają 256-bitowym kluczom ECC (32 bajty), lub 2048-bitowym kluczom RSA). Rozmiar kluczy i podpisów stanowi kluczową zaletę ECC względem innych rozwiązań w przypadku transmisji radiowej takiej jak LoRa, ze względu na mocno ograniczony rozmiar ładunku i restrykcyjny limit _duty cycle_. Inną zaletą krzywych eliptycznych jest to że na mikrokontrolerach które nie posiadają akceleracji sprzętowej do algorytmów takich jak RSA, są one dużo bardziej wydajne, zarówno w kwestii energii jak i cykli procesora.

\ Standardem krzywych eliptycznych jest krzywa *Curve25519* (TLS 1.3, Signal, OpenSSH, Reticulum, WireGuard), a protokołem uzgadniania kluczy *X25519* (RFC 7748), schematem podpisów cyfrowych natomiast *Ed25519* (RFC 8032, NIST FIPS 186-5). Innymi opcjami są krzywe NIST takie jak P-256 lub secp256r1, *Curve25519* jest jednak od nich lepsza w następujących dziedzinach:
- Zdefiniowana jest jako krzywa Montgomery'ego $y^2 = x^3 + 486662x^2 + x$ nad ciałem pierwszym o specjalnej postaci $p = 2^255 - 19$ co przyspiesza i upraszcza redukcję modularną. Badania implementacji pokazują 5-krotne skrócenie czasu wykonania względem NIST P-256 na mikrokontrolerach @dull_2015 @cortex_m4_curve25519.
- Wymiana kluczy X25519 używa algorytmu *drabinki Montgomery'ego*, który operuje tylko na współrzędnej $x$ punktu. Każdy krok algorytmu wykonuje stałą sekwencję operacji, jednego podwajania, jednego różnicowego dodawania, niezależnie od danych wejściowych, co zapewnia bezpieczeństwo przeciwko atakom _side channel_ @pornin_x25519.
- *Ed25519* bazuje na równoważnej krzywej skręconej Edwardsa o kompletnych wzorach dodawania (oznacza to że działają one bez wyjątków dla dowolnie wybranych punktów na krzywej), wprowadza on również parametr losowy za pomocą SHA-512, aby uniknąć ryzyka wycieku klucza w przypadku słabego generatora liczb losowych (wada z ECDSA).

#figure({
  set par(justify: false)
  set text(size: 9.5pt)
  set par(leading: 0.55em)
  table(
    columns: (auto, auto, auto, auto),
    table.header([*Implementacja*], [*Architektura*], [*Cykle CPU*], [*Charakterystyka pomiaru*]),
    [*X25519* (Düll i inni, 2015 @dull_2015)], [ARM Cortex-M0], [3 589 850 cykli], [Asembler ARMv6-M, około 8 KiB kodu],
    [*X25519* (Pornin, x25519-cm0 @pornin_x25519)], [ARM Cortex-M0 / M0+], [3 229 950 cykli], [Pełna zgodność z RFC 7748, i odporność na _side channel attacks_],
    [*Ed25519 podpis*], [ARM Cortex-M0], [$approx$3 000 000 cykli], [Wersja bez tablic prekomputowanych],
    [*Ed25519 weryfikacja*], [ARM Cortex-M0], [$approx$5 700 000 cykli], [Wersja zoptymalizowana pod rozmiar kodu]
  )
  },
  caption: [Porównanie kosztu obliczeniowego poszczególnych implementacji X25519 oraz Ed25519 @dull_2015 @pornin_x25519]
)

=== ChaCha20-Poly1305 vs AES-GCM i szyfrowanie symetryczne

Nowoczesne protokoły takie jak TLS 1.3, WireGuard, QUIC, bazują integralność i poufność danych na dwóch metodach enkrypcji *AEAD*, *AES-GCM* i *ChaCha20-Poly1305*. Pomimo tego że oba algorytmy realizują ten sam cel, to na prostych mikrokontrolerach bez sprzętowej akceleracji kryptograficznej *ChaCha20-Poly1305* jest bezpieczniejszy i szybszy niż *AES-GCM* @aes_gcm_iot_2019 @palii_chacha20_2026.

\ Największą wadą programowej implementacji AES są ataki czasowe (_timing attacks_). W celu przyspieszenia działania AES na procesorach 32-bitowych, operacje łączy się w duże tablice przeglądowe (_T-tables_). Ponieważ indeksy odczytów z tabeli zależą od klucza, minimalne wahania w czasie dostępu do pamięci mogą ujawnić wartość klucza @bernstein_cache_2005 (podatności w wolfSSL do wersji 5.6.5 na mikrokontrolerach Cortex-M). Zabezpieczanie programowe AES przed tego typu atakami wymaga skomplikowanych technik (_bitslicing_) które znacząco spowalniają kod. Szyfr *ChaCha20* w pełni eliminuje ten problem, jest to algorytm z rodziny ARX (_Addition-Rotation-XOR_) które wykorzystują tylko proste operacje arytmetyczno-logiczne: dodawanie, przesunięcie cykliczne, oraz operację XOR. Nie korzysta on również z tablic przeglądowych, i nie posiada rozgałęzień, co sprawia że czas wykonania *ChaCha20* jest stały (_constant-time_), zapewnia to odporność na ataki typu _side-channel_ bez spowalniania działania programu @palii_chacha20_2026.

\ Drugim powodem dlaczego *ChaCha20-Poly1305* jest lepszym wyborem na mikrokontrolerach jest dużo niższy koszt wykonania algorytmu. Funkcja sprawdzająca integralność w AES-GCM (GHASH) wymaga operacji w ciele $"GF"(2^128)$, proste mikrokontrolery nie posiadają akceleracji sprzętowej tych operacji, więc muszą je emulować co jest od dwóch do czterech razy wolniejsze niż szyfrowanie, i znacząco spowalnia układ, nawet w przypadku w którym mikrokontroler posiada akcelerację AES @aes_gcm_iot_2019.

\ Algorytm *Poly1305* bazuje na zwykłej arytmetyce liczb całkowitych (modulo $2^130 - 5$), są to podstawowe instrukcje w procesorach 32-bitowych. Dzięki temu na procesorach ARM bez akceleracji sprzętowej, *ChaCha20-Poly1305* działa do 3 razy szybciej niż AES-GCM. Z tego powodu standard TLS 1.3 zaleca *Poly1305* jako preferowany algorytm dla urządzeń bez sprzętowej akceleracji AES @palii_chacha20_2026.

\ Należy pamiętać że żaden z tych algorytmów nie jest odporny na użycie tego samego wektora _nonce_ dwa razy, w przypadku losowej generacji wektora zalecane jest użycie *XChaCha20-Poly1305* gdzie _nonce_ ma rozmiar 192 bitów ze względu na to że z 96-bitowy nonce algorytmu ChaCha20-Poly1305 posiada ryzyko kolizji ze względu na paradoks urodzin, wymagane jest tylko około $2^32"-"2^48$ wiadomości z losowo generowanym wektorem, aby wystąpiła kolizja @palii_chacha20_2026.

=== Problem wymiany kluczy bez serwera centralnego: TOFU i podpisy cyfrowe

Nawet bezbłędna implementacja *X25519* oraz *ChaCha20-Poly1305* nie chroni przed atakiem _Man-in-the-middle_ (MITM), przez brak możliwości powiązania klucza publicznego z tożsamością. Rozproszone sieci _off-grid_ z definicji nie mają możliwości użycia centralnego serwera uwierzytelniającego.

\ W rezultacie w niezależnych sieciach _off-grid_ stosuje się model *Trust On First Use (TOFU)* obecny między innymi w protokole SSH lub komunikatorach takich jak Signal. Model TOFU polega na zapisaniu klucza publicznego rozmówcy podczas pierwszego nawiązania połączenia, i od tego momentu domyślnie ufa mu, następnie przy każdej kolejnej interakcji, klucz jest sprawdzany, i w przypadku gdy się nie zgadza, użytkownik otrzymuje komunikat o potencjalnych ataku MITM. Pomimo tego że model TOFU zapewnia wysoką użyteczność w sieciach zdecentralizowanych, istnieje w nim jedna krytyczna podatność, czyli podmiana klucza w trakcie pierwszej wymiany @sok_secure_messaging_2015.

\ W celu wyeliminowania tej podatności, stosuje się trzy uzupełniające się mechanizmy:
- *Odciski kluczy (fingerprints)* - skrót klucza publicznego, prezentowany za pomocą znaków lub kodu QR. Porównanie ich poprzez zeskanowanie twarzą w twarz, weryfikuje zaufanie.
- *Krótkie ciągi uwierzytelniające (SAS - Short Authentication Strings)* - metoda z protokołu ZRTP, gdzie obie strony odczytują na głos ciąg znaków wynikający z wcześniej uzgodnionego sekretu co łączy kryptografię z biometrią.
- *Podpisy cyfrowe* - długoterminowy klucz, który służy do podpisywania kluczy wymiany lub zaufanych urządzeń. Pozwala nam to ograniczyć ilość ręcznych weryfikacji odcisków w zespołach z wieloma urządzeniami. Sam podpis jednak gwarantuje jedynie ciągłość posiadania klucza, i nie zastępuje potwierdzenia tożsamości.
- *Wspólne hasła (PSK - Pre-Shared Key)* - Polega to na tym że przed wdrożeniem członkowie zespołu ustalają poza pasmem wspólne hasło, za pomocą którego generowany jest klucz szyfrujący przy użyciu funkcji wyprowadzania kluczy (Argon2, KDF, HKDF). W nowoczesnych rozwiązaniach używa się również protokołów PAKE (Password-Authenticated Key Exchange), dzięki którym możemy uwierzytelnić wymianę kluczy asymetrycznych za pomocą krótkiego hasła nie ujawniając go w sieci. Jednym z minusów tego rozwiązania jest konieczność bezpiecznego rozpowszechnienia sekretu przed połączeniem, oraz utrata poufności całej sieci w sytuacji w której jedno urządzenie zostanie przejęte, a klucz odczytany z pamięci @sok_secure_messaging_2015.

\ Ważnym wnioskiem z badań użyteczności zabezpieczeń (_"SoK: Secure Messaging"_ @sok_secure_messaging_2015 i _"Secure Messaging Authentication Ceremonies Are Broken"_ @vazquez_ceremonies_2020) jest to, że jeśli weryfikacja użytkowników ukryta jest w menu, to niemal nikt jej nie używa, i nie rozumie zagrożenia MITM. Dopiero w sytuacji gdy interfejs użytkownika jasno mówi że kontakt jest niezweryfikowany, zrozumienie ceremonii weryfikacji wzrosło z 16% do 50%, a znaczenie zaufanych identyfikatorów z 27% do 56%, należy więc pamiętać że przy projektowaniu urządzeń _off-grid_ mechanizm zaufania kontaktów powinien być jasnym i integralnym elementem parowania urządzeń @vazquez_ceremonies_2020.

= Wybrane algorytmy

== Kryteria doboru stosu technologicznego

Najważniejszą częścią doboru stosu technologicznego w systemach wbudowanych jest wzięcie pod uwagę ograniczeń które ograniczają projekt. W przypadku transmiji LoRa najważniejszym ograniczeniem jest rozmiar pojedynczego pakietu, który wynosi 255 bajtów, oraz regulacje prawne, które mocno ograniczają czas nadawania i limity mocy. Sprawia to że każdy bajt podczas transmisji jest jeszcze bardziej cenny niż w innych typach transmisji, dużym wyzwaniem jest więc uzyskanie silnej kryptografii względem zużycia baterii i czasu trwania transmisji (_Time on Air_). W projekcie wykorzystano moduł SX1262 który posiada konfigurowalne parametry:
- Bandwidth w zakresie 7.81 - 500 kHz
- Spreading Factor w zakresie 5 - 12
- Kodowanie w zakresie 1 - 4
- Częstotliwość w zakresie 410 - 510 MHz
- Moc w zakresie -9 dBm - +22 dBm 

\ Parametry zostały wybrane tak aby osiągnąć złoty środek, BW = 125 kHz aby zachować czułość odbiornika na tanim układzie, SF = 9 w celu przyspieszenia transmisji, kodowanie 4/5 dla podstawowej korekcji błędów, standardową preambułę 8 pakietów oraz moc 10 dBm ze względu na normy europejskie. 

\ Ze względu na brak sprzętowej akceleracji AES oraz ograniczoną pamięć SRAM, w projekcie zrezygnowano z bibliotek takich jak mbedTLS czy wolfSSL na rzecz lżejszej biblioteki *Monocypher*, napisana jest ona w standardzie C99. Dzięki temu wydajność kryptografii na rdzeniu ARM Cortex-M0+ jest dużo lepsza. Główne zalety *Monocypher* to:
- Odporność na _side-channel attacks_, wszystkie krytyczne operacje kryptograficzne zaimplementowane są w sposób stałoczasowy (_constant-time_), bez rozgałęzień programu, i indeksowania tablic na podstawie sekretów, co chroni program przed analizą czasową (_timing attacks_).
- Biblioteka implementuje wydajne algorytmy na architekturze 32-bitowej, algorytmy z rodziny ARX (_Addition-Rotation-XOR_) takie jak ChaCha20 oraz BLAKE2b, bazują one na prostych 32-bitowych operacjach arytmetyczno-logicznych, co zapewnia nieporównywalnie lepszą wydajność na procesorach Cortex-M0+ w stosunku do programowych implementacji AES.
- Brak dynamicznej alokacji pamięci (malloc oraz free) i operuje na buforach przekazanych przez program główny, które są alokowane statycznie lub na stosie. Całkowicie eliminuje to zjawisko wycieków pamięci przy długotrwałej pracy urządzenia.
- Łatwość integracji w projekcie zapewniana jest przez sposób w jaki biblioteka jest rozpowszechniana, czyli para dwóch plików źródłowych (monocypher.c i monocypher.h) bez innych zależności co zmniejsza rozmiar końcowego programu oraz upraszcza jego kompilację.

== Zastosowane algorytmy kryptograficzne

=== Generowanie kluczy oraz tożsamości urządzenia

#figure(
  image("images/diagram1.svg"),
  caption: [Schemat wyprowadzenia kluczy urządzenia (Ed25519 i X25519) oraz odcisku tożsamości (BLAKE2b)]
)

#figure(
  image("images/diagram2.svg"),
  caption: [Schemat wyprowadzenia symetrycznego klucza sesyjnego]
)

#import "@preview/bytefield:0.0.9": *

#figure(
  bytefield(
    bpr: 32,
    group(right, 12)[AAD (75 B) -- uwierzytelniane],
    bytes(8, fill: luma(235))[Header `LCL1` (27 B)],
    bytes(8, fill: luma(235))[Nick (16 B)],
    bytes(8, fill: luma(235))[Fingerprint (8 B)],

    group(left, 6)[Wektor Nonce (24 B)],
    bytes(8, fill: rgb("#fff2cc"))[`SenderID` (8 B)],
    bytes(8, fill: rgb("#fff2cc"))[`RecipientID` (8 B)],
    bytes(4, fill: rgb("#ffe699"))[`Sequence` (4 B)],
    bytes(4, fill: luma(225))[`0x00000000`(4B)],
    
    group(right, 4)[AEAD],
    bytes(8, fill: rgb("#e8f0fe"))[MAC Poly1305 (16 B)],
    bytes(8, fill: rgb("#d5e8d4"))[Data XChaCha20 (1-96 B)],
  ),
  caption: [meow]
)

// AI NOTE

  MAC i AAD chronią przed 4 konkretnymi zagrożeniami:

  1. Manipulacja treścią (bit-flipping):
  Sam szyfr strumieniowy (XChaCha20) bez MAC nie wykrywa zmian — atakujący mógłby odwrócić wybrane bity w szyfrogramie (np. zamienić znane fragmenty tekstu), a odbiorca by to odszyfrował. MAC odrzuca pakiet
  przy zmianie choćby jednego bitu.
  2. Fałszowanie wiadomości (spoofing):
  Bez znajomości klucza sesji nikt nie potrafi wyliczyć poprawnego 16-bajtowego tagu Poly1305, więc nie podszyje się pod nadawcę.
  3. Podmiana jawnych metadanych (to robi AAD):
  Ponieważ MAC obejmuje również 75 B jawnego AAD, atakujący nie może w locie zmienić np. SenderID, RecipientID, Sequence, Nickname ani Fingerprint w nagłówku ramki — każda taka zmiana unieważnia tag MAC.
  4. Niewykryte błędy transmisji radiowej:
  Gdyby w silnie zaszumionym kanale LoRa sprzętowe CRC-16 przepuściło uszkodzony pakiet, 128-bitowy MAC gwarantuje odrzucenie uszkodzonych danych.

  (Warto wiedzieć na obronę: sam MAC + AAD nie chroni przed atakiem powtórzeniowym (Replay Attack) — czyli nagraniem poprawnego pakietu i wysłaniem go drugi raz — do tego służy sprawdzanie monotoniczności
  licznika sequence po stronie odbiorcy).

// AI NOTE

== Protokół parowania urządzeń

#figure(
  image("images/diagram3.svg"),
  caption: [Schemat protokołu parowanai urządzeń]
)

== Protokół wymiany wiadomości oraz ACK

== Algorytm transferu plików

= Implementacja

== Architektura sprzętowa

// AI NOTE!!!!
2. Platforma sprzętowa: RP2040 vs ESP32 vs STM32:
  • Dlaczego RP2040: 2 rdzenie Cortex-M0+ (133 MHz), elastyczne PIO, brak wbudowanego prądożernego radia Wi-Fi/BT (niższy pobór w stanie czuwania niż ciągle aktywny ESP32), dojrzałe Pico SDK w C++,
  niski koszt.

  Natomiast w kolejnych sekcjach zdecydowanie warto dodać jeszcze te 5 diagramów, bo każda z nich ma inną dynamikę:
  
  2. 4.4 (Wiadomości i ACK): Diagram sekwencji UML — wysłanie EncryptedMessage, oczekiwanie z timeoutem, MessageAck vs Failed.
  3. 4.5 (Transfer plików): Diagram sekwencji UML (4 aktorów) — Host A ↔ RP2040 A ↔ RP2040 B ↔ Host B (FileExchangeStart z fileKey w AEAD + pętla FileExchangeData w ChaCha20-IETF + TxDone/FileExchangeAck).
  4. 5.1 (Architektura sprzętowa): Schemat blokowy połączeń — RP2040 ↔ SX1262 (SPI), ILI9341 (SPI), CardKB (I2C), PC (USB CDC / UART).
  5. 5.3 (Maszyny stanów): Diagram stanów (stateDiagram-v2) — główne widoki UI (Chat, Contacts, Invite, Identity) oraz stany radia/transferu (Idle / AwaitingAck / SendingFile / ReceivingFile).
  Łącznie wyjdzie Ci 8 konkretnych diagramów na całą pracę — każdy inny, zero powtórek i dokładnie w miejscach, o które pyta komisja na obronie.
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

2. Raspberry Pi Ltd. – RP2040 Datasheet oraz Raspberry Pi Pico C/C++ SDK.
3. L. Louédic (Monocypher) – Oficjalna dokumentacja i specyfikacja biblioteki kryptograficznej Monocypher.
4. RFC 7748 – Elliptic Curves for Security (Krzywe Curve25519 / X25519).
5. RFC 8439 – ChaCha20 and Poly1305 for IETF Protocols.
6. Artykuły naukowe z IEEE Xplore dotyczące analizy zasięgu modulacji LoRa (hasła: LoRa path loss models, LoRa CSS performance under noise).
7. Normy ETSI dotyczące wykorzystania pasm nielicencjonowanych ISM w Europie (Short Range Devices - SRD).
// AI NOTE!!!

== Bibliografia

#bibliography("biblio/bibliografia.bib", title: "Bibliografia", style: "ieee")