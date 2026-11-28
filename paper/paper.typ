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

Ogromnym problemem jest podatność tej infrastruktury. Pomimo ciągłego ulepszania i unowocześniania jej, nadal jest ona dość krucha. Wichury, powodzie, opady śniegu oraz awarie prądu natychmiast sprawiają, że stacje GSM stają się bezużyteczne. W przypadku wybuchu konfliktu zbrojnego oczywistym jest, że tak krytyczna infrastruktura będzie jednym z pierwszych celów, aby upośledzić koordynację kraju w tak kluczowym momencie.

Rośnie również świadomość dotycząca cyberbezpieczeństwa oraz architektury systemów, tego czy nasze dane przechowywane są bezpiecznie, scentralizowane systemy niosą ze sobą ryzyko punktów pojedynczej awarii (Single point of failure), jak i również prywatności, czyli podatność ruchu sieciowego na analizę i profilowanie oraz to, kto i jak przechowuje nasze dane, i komu je udostępnia.

Jak więc rozwiązać te problemy? Praktyczna użyteczność bezinfrastrukturowej komunikacji zależy od tego na ile jest ona bezpieczna, wygodna w użytkowaniu, przystępna cenowo oraz niezależna energetycznie. Tym problemem zajmuje się niniejsza praca, której celem jest implementacja przenośnego oraz niezależnego urządzenia do szyfrowanej komunikacji, które łączy zalety modulacji LoRa ze współczesnymi rozwiązaniami kryptografii.

#pagebreak()

= Wprowadzenie

== Cel pracy

Celem niniejszej pracy jest projekt oraz implementacja autonomicznego przenośnego urządzenia (handheld) do bezprzewodowej komunikacji tekstowej na dalekie dystanse, bazującego na technologii LoRa oraz mikrokontrolerze RP2040. Urządzenie umożliwia bezpieczną komunikację za pomocą wiadomości tekstowych oraz na wymianę plików, cały ruch szyfrowany jest przed emisją radiową, co zapewnia wysoki poziom poufności oraz integralność przesyłanych wiadomości. Urządzenie działa w pełni niezależnie, bez dostępu do publicznej infrastruktury sieciowej (komunikacja off-grid).

== Zakres

Aby zrealizować ten cel zdefiniowano następujący zakres pracy:
+ Przegląd literatury na temat technologii LoRa oraz bezpiecznej komunikacji w systemach wbudowanych.
+ Projekt i budowa prototypu urządzenia (mikrokontroler RP2040, moduł radiowy, wyświetlacz LCD, klawiatura)  oraz schemat połączeń.
+ Opracowanie protokołu transmisji oraz komunikacji z mechanizmami szyfrowania End-to-End.
+ Implementacja firmware'u urządzenia, realizującego obsługę modułu LoRa, sterowników peryferiów, interfejsu użytkownika oraz pamięci trwałej.
+ Implementacja oprogramowania na komputer do komunikacji z urządzeniem.
+ Przeprowadzenie testów działania urządzenia oraz pomiarów zasięgu i jakości sygnału.

== Struktura

Układ pracy jest podporządkowany założonemu zakresowi pracy w następujący sposób:

- *Rozdział 1 (Wstęp)* przedstawia powód podjęcia się tematu, przedstawia zagrożenia związane z centralizacją i awaryjnością infrastruktury GSM.
- *Rozdział 2 (Wprowadzenie)* zawiera szczegółowy cel pracy, zakres oraz strukturę.
- *Rozdział 3 (Stan wiedzy)* eksploruje literaturę na temat modulacji LoRa, regulacji prawnych, istniejących rozwiązań oraz kryptografii na mikrokontrolerach.
- *Rozdział 4 (Wybrane algorytmy)* prezentuje powody doboru parametrów transmisji LoRa oraz biblioteki Monocypher, jak i opisuje kryptografię wykorzystaną w projekcie, szyfrowanie wiadomości, wyprowadzenie tożsamości urządzenia, protokół parowania, mechanizm potwierdzenia odbioru wiadomości oraz protokół transferu plików.
- *Rozdział 5 (Implementacja)* opisuje architekturę prototypu, użyte środowisko programistyczne, wybrane maszyny stanów oraz warstwę interfejsu użytkownika (TUI, LCD, USB CDC).
- *Rozdział 6 (Testy i walidacja)* przedstawia uzyskane efekty takie jak zasięg oraz wydajność obliczeniową urządzenia.
- *Rozdział 7 (Wnioski)* podsumowuje realizację prototypu, przedstawia napotkane ograniczenia zastosowanych rozwiązań oraz kierunki dalszego rozwoju.

= Stan wiedzy

== Modulacja LoRa i pasma ISM

=== Czym jest modulacja CSS?

LoRa (Long Range) to technologia transmisji radiowej opracowana przez firmę Semtech, polega ona na modulacji z rozproszonym widmem sygnałem świergotowym (CSS _Chirp Spread Spectrum_) @semtech_an1200. Główną zasadą działania tego mechanizmu jest wymiana przepustowości na zysk przetwarzania (processing gain) oraz odporność na szum otoczenia (background noise).

\ Przesyłanie danych odbywa się poprzez tak zwane "chirpy" czyli przebieg którego częstotliwość zmienia się w czasie w sposób liniowy i ciągły, przechodząc przez całe pasmo kanału @semtech_an1200.
\ Przebiegi dzielimy na dwa rodzaje, czyli "up-chirp" oraz "down-chirp", jeśli przebieg zaczyna się na dolnej granicy pasma ($F_c - (B W)/2$) a kończy na górnej ($F_c + (B W)/2$) a nachylenie jest dodatnie ($beta > 0$) to jest to "up-chirp", jeśli natomiast przebieg zaczyna się na górnej granicy a kończy na dolnej z nachyleniem ujemnym ($beta < 0$) to jest to "down-chirp".
\ Dwa rodzaje sygnałów dają tylko dwie wartości które można przesłać, dlatego LoRa wykorzystuje kodowanie M-arne, w którym $M = 2^("SF")$ a pojedynczy symbol zawiera $"SF"$ bitów danych @semtech_an1200.
Aby móc reprezentować symbol z zakresu $s in {0, 1, ... 2^("SF")-1}$ bazowy "up-chirp" podlega cyklicznemu przesunięciu czasowemu (cyclic shift) zależnie od wartości $s$. W efekcie na wykresie częstotliwości względem czasu sygnał zaczyna się od częstotliwości odpowiadającej symbolowi $s$, liniowo narasta do górnej granicy pasma, a następnie "zawija" się z powrotem do dolnej krawędzi pasma i kontynuuje aż do częstotliwości startowej.

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

\ gdzie $N_"pre"$ to liczba sygnałów preambuły, PL to ilość przesyłanych bajtów danych, $"CR"_"idx"$ to liczba $"idx" in {1,2,3,4}$ odpowiadająca odpowiednio dla CR 4/5-4/8, parametr DE jest równy 1 przy włączonej optymalizacji niskich przepustowości (stosuje się ją dla SF11 i SF12 przy BW 125 kHz), stała 44 odpowiada włączonemu nagłówkowi ($"IH" = 0$) i włączonej sumie kontrolnej ładunku ($"CRC" = 1$). Według tego wzoru pakiet o długości 20 bajtów, z podstawowym kodowaniem CR 4/5, BW 125 kHz oraz SF10, będzie przesyłany przez 0.370688 sekund, lub 370.688 milisekund.

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
      [Odwrotnie proporcjonalny do $T_"sym"$ (Podwojenie BW skraca czas trwania symbolu o połowę)],
      [Szersze pasmo przyspiesza prędkość transmisji, ale wpuszcza do odbiornika więcej szumu tła, obniżając czułość],

      [Coding Rate (CR)],
      [4/5, 4/6, 4/7, 4/8],
      [Zwiększa rozmiar pakietu proporcjonalnie do wybranego kodowania],
      [Wyższe wartości zwiększają nadmiarowość dla poprawy odporności na błędy, ale zwiększają ToA],
    )
  },
  caption: [Parametry modulacji LoRa i ich efekt na transmisję]
)

=== Czułość odbiornika poniżej progu szumów i duty cycle

LoRa pozwala na dekodowanie sygnałów poniżej progu szumów poprzez skupienie energii rozproszonego sygnału w jednym wąskim prążku podczas "de-chirpingu", teoretyczną czułość odbiornika w dBm wyznacza równanie @semtech_an1200[s. 13]:
$ S_"RX" = -174 + 10log_10("BW") + "NF" + "SNR"_"limit"("SF") $

\ Gdzie -174 dBm/Hz to gęstość widmowa szumów termicznych w temperaturze pokojowej, BW w hercach, NF to współczynnik szumów odbiornika (dla SX1276 jest to $"NF" = 6 "dB"$), $"SNR"_"limit"$ to minimalny stosunek sygnału do szumu otoczenia, który wymagany jest, aby poprawnie zdekodować sygnał. Oficjalne dane producenta (Semtech) informują, że próg ten wynosi $-7.5"dB"$ dla $"SF7"$ oraz $"-20 dB"$ dla $"SF12"$ @sx1276_datasheet[Tabela 13, s. 27].

\ W przypadku standardowej transmisji z parametrami $"BW"= 125 "kHz" (10log_10(125000) approx 50.97 "dB")$ oraz $"NF"= 6 "dB"$ otrzymujemy podłogę szumową (noise floor) wynoszącą około $-117.03 "dBm"$. W rezultacie teoretyczna czułość wynosi około $-124.5 "dBm"$ dla SF7 oraz około $-137.0 "dBm"$ dla SF12. Jeśli dodatkowo zawężymy pasmo i wybierzemy najwyższą wartość SF12, układ SX1276 osiągnie maksymalną czułość katalogową  $-148 "dBm"$ @sx1276_datasheet[s. 1]. Budżet łącza (Link Budget, jednostka dB) wyznacza równanie @semtech_an1200[s. 15--16]:
$ "Link Budget" = P_"TX" + G_"TX" + G_"RX" - L_"loss" - S_"RX" $

\ gdzie $P_"TX"$ to moc nadajnika, $G_"TX"$ oraz $G_"RX"$ to zyski anten odpowiednio nadajnika i odbiornika, $L_"loss"$ to suma strat kabli i złączy. Maksymalna moc SX1276 wynosi $+20 "dBm" (100 "mW")$ a czułość $-148 "dBm"$, więc teoretyczny budżet łącza sięga $168 "dB"$, a dla standardowych parametrów transmisji (+20 dBm, SF10, BW 125) wynosi $152 "dB"$  (przy zerowych stratach). W realnych warunkach jest on jednak niższy ze względu na regulacje prawne określające limity mocy nadawania.

=== Regulacje prawne w Europie: pasma EU433 i EU868

Zasady użytkowania pasm ISM (_Industrial, Scientific, Medical_) i SRD (ang. _Short Range Devices_) w Europie reguluje norma ETSI EN 300 220 @etsi_300_220_2 zalecenie CEPT ERC/REC 70-03. W Polsce tymi normami zajmuje się Ministerstwo Cyfryzacji, poprzez "rozporządzenie Ministra Cyfryzacji w sprawie rozszerzonego zakresu urządzeń radiowych nadawczych lub nadawczo-odbiorczych, które mogą być używane bez pozwolenia radiowego. Na podstawie art. 145 ust. 4 ustawy z dnia 12 lipca 2024 r" @bip_uke.

\ Pasmo EU433 obejmuje zakres częstotliwości 433.050-434.790 MHz, a pasmo EU868 obejmuje 863-870 MHz, różnią się one maksymalną dopuszczalną mocą transmisji oraz regułami czasu dostępu do widma (cykl pracy i techniki LBT+AFA, Listen Before Talk + Adaptive Frequency Agility, nazwane w normie "polite spectrum access") @etsi_300_220_2[Table B.1, s. 21--22].

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
  caption: [Zestawienie parametrów i pasm EU433 oraz EU868 bazujące na normie ETSI EN 300 220-2 @etsi_300_220_2[Tabela B.1].]
)

\ W rezultacie normy te wymuszają wprowadzenie programowego ograniczenia mocy nadajnikowo-odbiorników (SX1276 jest w stanie nadawać z mocą +20 dBm) do +10 dBm w przypadku EU433, lub +14 dBm dla EU868, dużym problemem jest również limit _Duty Cycle_, który w przypadku 1% daje urządzeniu tylko 36 sekund aktywnej transmisji na godzinę. W przypadku standardowych parametrów transmisji z podstawowym kodowaniem CR 4/5, BW 125 kHz oraz SF10, a więc ToA rzędu 371 milisekund (dla 20 bajtów danych) otrzymujemy możliwość wysłania pojedynczego pakietu około raz na 37.1 sekund ($approx 97$ pakietów na godzinę), w przypadku wykorzystania techniki "meshowania", która wymaga retransmitowania ruchu innych użytkowników, to właśnie _Duty Cycle_ stanowi największe wąskie gardło przepustowości.

== Przegląd istniejących rozwiązań łączności P2P

=== LoRaWAN 

LoRaWAN to otwarty (open-source) protokół operujący w warstwie MAC i warstwie sieciowej zarządzanym przez LoRa Alliance, działa on bezpośrednio nad fizyczną warstwą LoRa @lorawan_energy_tutorial[s. 5]. Architektura LoRaWAN bazuje na topologii *gwiazdy gwiazd* (_stars-of-stars_), w której urządzenia końcowe (_End Devices_, ED) wysyłają pakiety tylko w komunikacji jednoskokowej (_single-hop_) za pomocą niesynchronizowanego protokołu ALOHA do bramek (_Gateways_, GW). Bramki pełnią tylko rolę mostów przekazujących pakiety LoRa poprzez łącze IP (poprzez protokół MQTT) do infrastruktury serwerowej @lorawan_energy_tutorial[s. 5]. 

\ Siecią LoRaWAN zarządzają trzy komponenty serwerowe @lorawan_energy_tutorial[s. 10--11]:
- *Network Server (LNS/NS)* odpowiedzialny jest za deduplikację tych samych pakietów które zostały odebrane przez różne bramki, jak i zarządzanie warstwą MAC.
- *Join Server (JS)* zajmuje się przechowywaniem kluczy głównych (_NwkKey_, _AppKey_) oraz obsługuje procedurę uwierzytelniania i generuje klucze sesji.
- *Application Server (AS)* zajmuje się odbieraniem i deszyfrowaniem danych aplikacji.

\ Zastosowanie LoRaWAN w celu autonomicznej komunikacji _off-grid_ uniemożliwia między innymi @lorawan_energy_tutorial:
- Brak wieloskokowego routingu pomiędzy węzłami końcowymi, w efekcie utrata zasięgu do bramki sprawia, że urządzenie zostaje sparaliżowane, nowsze rozszerzenia protokołu wprowadzają "węzły przekaźnikowe (_relay_)" ograniczone do maksymalnie 16 urządzeń na przekaźnik i wyłącznie przekazywanie pakietów w stronę bramki @lorawan_hybrid_2025[s. 3].
- LoRaWAN wymaga kosztownych bramek i sprawnego, stałego łącza do serwerów LNS i JS, Join Server stanowi aktualnie pojedynczy punkt awarii (_single point of failure_) jeśli chodzi o uwierzytelnianie i bezpieczeństwo.
- Klasy pracy protokołu (Klasa A, B, C) zostały zaprojektowane z myślą o wysyłaniu asymetrycznej telemetrii do chmury, energooszczędna Klasa A posiada krótkie okno odbiorcze wyłącznie po własnej transmisji, co uniemożliwia asynchroniczną wymianę wiadomości _peer-to-peer_, natomiast ciągłe nasłuchiwanie w Klasie C, jest bardzo energochłonne i zmniejsza czas życia urządzenia na baterii @lorawan_energy_tutorial[s. 10--11].

=== Meshtastic i routing mesh flooding

Meshtastic jest otwartym projektem typu _mesh_, który wykorzystuje technologię LoRa w nielicencjonowanych pasmach ISM w celu komunikacji tekstowej, udostępniania pozycji GPS, lub telemetrii bez użycia infrastruktury GSM @meshtastic_docs. Meshtastic nie korzysta z bramek lub serwerów centralnych, protokół implementuje algorytm *managed flooding*, polega on na kontrolowanym zalewaniu sieci z jednoczesnym tłumieniem retransmisji. Dzięki temu urządzenia nie muszą przechowywać tablic routingu ani topologii, każdy pakiet posiada parametr _hop limit_, który jest zmniejszany o jeden po każdym przekazaniu aż osiągnie wartość 0, urządzenia opóźniają transmisję pakietów, i rezygnują z niej całkowicie jeśli odbiorą, że dany pakiet został retransmitowany.

\ Największą zaletą Meshtastic jest gotowość do pracy od razu po włączeniu urządzenia, bez konfiguracji oraz jedna z największych społeczności wśród otwartych sieci LoRa, co czyni go najlepszym i najłatwiej dostępnym kandydatem do szybkiego wdrożenia w sytuacji kryzysowej, w obszarze pozbawionym tradycyjnej infrastruktury telekomunikacyjnej. Nie jest on jednak pozbawiony problemów, najważniejsze z nich to:
- Ograniczona skalowalność i zatory, LoRa posiada przepustowość rzędu kilkuset bitów na sekundę, przez to, że kanał jest współdzielony w przypadku większych lub gęstych sieci mechanizm _floodingu_ powoduje kolizje, gubienie pakietów (_packet loss_) oraz opóźnienia @croce_2018[s. 1--3].
- Restrykcje Duty Cycle powodują, że pomimo wolnego kanału, urządzenia są zmuszone przestać nadawać, przez co nie mogą retransmitować pakietów, i następuje fragmentacja topologii sieci.
- Limit liczby przeskoków oraz nieoptymalne przesyłanie pakietów, Meshtastic nie optymalizuje kierunku przesyłu pakietów i po prostu przesyła każdy pakiet dalej, co w rezultacie skutkuje przesyłaniem pakietów w kierunku przeciwnym do odbiorcy docelowego, jak i ograniczony zasięg komunikacji.

=== Reticulum Network Stack i Disaster Radio

*Reticulum Network Stack (RNS)* to zaprojektowany przez Marka Qvista stos sieciowy skupiający się na kryptografii, działa on w przestrzeni użytkownika (_userland_) i nie wykorzystuje protokołu IP (istnieje jednak możliwość tunelowania ruchu przez TCP/UDP) @reticulum_manual. Kluczową zaletą RNS jest to, że działa na dużej liczby typów łącz. Jeden system automatycznie bez dodatkowej konfiguracji łączy moduły radiowe (LoRa/RNode, VHF/UHF, TNC/KISS), siecią IP poprzez Wi-Fi lub Ethernet, portami szeregowymi, stdio czy I2P, a jednocześnie automatycznie zarządza ruchem i daje priorytet dla komunikacji lokalnej.

\ Reticulum zamiast prostego floodingu implementuje wieloskokowe trasowanie z wydzielaniem węzłów transportowych (_transport nodes_), które pełnią funkcję rozproszonego magazynu kluczy i ścieżek @reticulum_manual, ważne jest, że nie każdy węzeł w sieci jest węzłem transportowym, ponieważ prowadzi to do degradacji wydajności. Adresowanie działa na podstawie zdecentralizowanych skrótach kluczy publicznych (destinations) a pakiety nie posiadają adresów nadawcy, co zapewnia anonimowość inicjatora. Cały ruch zabezpiecza szyfrowanie _end-to-end_ przy użyciu tymczasowych kluczy X25519. Aby nawiązać w pełni szyfrowaną sesję wymagane są zaledwie 3 pakiety, o łącznym rozmiarze 297 bajtów, a utrzymanie jej potrzebuje tylko 0.44 bps @reticulum_manual. RNS wykorzystuje między innymi:
- Nomad Network
- Protokół LXMF

\ Należy pamiętać, że autor sam ostrzega, że stos jest młody, i nie przeszedł jeszcze żadnego zewnętrznego audytu bezpieczeństwa.

\ *Disaster Radio* jest projektem który skupia się na budowie tanich, zasilanych solarnie, węzłów łączności kryzysowej, głównie na mikrokontrolerach ESP32 z modułem LoRa @disaster_radio_wiki. Największą cechą wyróżniającą projekt jest możliwość korzystania z aplikacji bez instalacji klienta, urządzenie rozgłasza sieć Wi-Fi w której pod adresem 192.168.4.1 udostępnia interfejs poprzez serwer HTTP oraz WebSockets (alternatywą jest aplikacja Android i łączność poprzez Bluetooth). Projekt korzysta z autorskiego protokołu na bazie LoRaLayer2, każdy węzeł posiada swój unikalny 4 bajtowy adres oraz dla każdego odebranego pakietu podejmuje decyzję o retransmisji. Projekt pozwala również na zapisywanie historii wiadomości na karcie SD @disaster_radio_wiki. 

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

Implementacja nowoczesnych algorytmów kryptografii na prostych, tanich oraz energooszczędnych mikrokontrolerach ARM Cortex-M0 oraz ARM Cortex-M0+ szybko napotyka problemy poprzez ograniczenia sprzętowe @dull_2015 @arissen_2015. Mikrokontroler Cortex-M0 posiada tylko 16 rejestrów 32-bitowych (R0-R15), trzy górne są zarezerwowane przez system i pełnią funkcje:
- R13 - wskaźnik stosu
- R14 - rejestr powrotu
- R15 - licznik programu

\ Pozostawia to 13 rejestrów dla programu, a w przypadku trybu "Thumb", instrukcje arytmetyczne mają dostęp tylko do rejestrów R0-R7 (rejestry R8-R12 dostępne są tylko dla niektórych instrukcji takich jak MOV, CMP, ADD, przez co często wymagany jest tak zwany _register spilling_), w celu przetwarzania 256-bitowych liczb. Drugim problemem są ograniczenia pamięciowe, popularny układ STM32F051 posiada tylko 8 KB pamięci RAM i 64 KB pamięci Flash.

\ Na wydajność obliczeniową tych mikrokontrolerów wpływa między innymi @arissen_2015[s. 5--9]:
- Zastosowanie architektury von Neumanna, która współdzieli pojedynczą magistralę zarówno dla instrukcji jak i danych, co sprawia ze operacje odczytu oraz zapisu pamięci (LDR / STR, jak i LDM / STM które wymagają n + 1 cykli procesora) wprowadzają opóźnienia, i sprawiają, że procesor musi czekać, aby pobrać następną instrukcję programu, w rezultacie liczba wykonywanych instrukcji na sekundę (MIPS) maleje. 
- Brak instrukcji DSP, układów wektorowych SIMD oraz mnożenia rozszerzającego UMULL, dostępne są one dopiero w droższych mikrokontrolerach Cortex-M4 @cortex_m4_curve25519.
- Dostępne jest jedynie mnożenie 32-bitowe MULS, w zależności od tego jak jest zaimplementowane wykonuje się w 1 cykl procesora, lub aż w 32 cykle procesora w układach zoptymalizowanych pod minimalną powierzchnię układu @dull_2015[s. 8].
- Brak koprocesorów AES oraz jednostek mnożenia bezprzeniesieniowego (carry-less multiplication) wykorzystywanych w celu uwierzytelniania GHASH @aes_gcm_iot_2019.

\ Dużym problemem jest również podatność na tak zwane _side channel attacks_. Czas wykonywania instrukcji może ulegać fluktuacjom z powodu konfliktu na magistrali podczas operacji DMA, oczekiwania na pamięć lub tego która gałąź programu została wybrana na podstawie tajnych bitów @bernstein_cache_2005. Każdy algorytm kryptograficzny powinien być realizowany w sposób stałoczasowy (_constant-time_), bez instrukcji warunkowych które powodują rozgałęzienie programu i tablic indeksowanych tajnymi danymi.

=== RSA vs ECC oraz Curve25519

W systemach wbudowanych oraz sieciach LoRa klasyczna kryptografia RSA została zastąpiona przez kryptografię krzywych eliptycznych (ECC), ECC osiąga poziom bezpieczeństwa podobny do tradycyjnego RSA, przy znacząco krótszych kluczach (128-bitowe klucze AES, odpowiadają 256-bitowym kluczom ECC (32 bajty), lub 2048-bitowym kluczom RSA). Rozmiar kluczy i podpisów stanowi kluczową zaletę ECC względem innych rozwiązań w przypadku transmisji radiowej takiej jak LoRa, ze względu na mocno ograniczony rozmiar ładunku i restrykcyjny limit _duty cycle_. Inną zaletą krzywych eliptycznych jest to, że na mikrokontrolerach które nie posiadają akceleracji sprzętowej do algorytmów takich jak RSA, są one dużo bardziej wydajne, zarówno w kwestii energii jak i cykli procesora.

\ Standardem krzywych eliptycznych jest krzywa *Curve25519* (TLS 1.3, Signal, OpenSSH, Reticulum, WireGuard), a protokołem uzgadniania kluczy *X25519* (RFC 7748), schematem podpisów cyfrowych natomiast *Ed25519* (RFC 8032, NIST FIPS 186-5). Innymi opcjami są krzywe NIST takie jak P-256 lub secp256r1, *Curve25519* jest jednak od nich lepsza w następujących dziedzinach:
- Zdefiniowana jest jako krzywa Montgomery'ego $y^2 = x^3 + 486662x^2 + x$ nad ciałem pierwszym o specjalnej postaci $p = 2^255 - 19$ co przyspiesza i upraszcza redukcję modularną. Badania implementacji pokazują 5-krotne skrócenie czasu wykonania względem NIST P-256 na mikrokontrolerach @dull_2015 @cortex_m4_curve25519.
- Wymiana kluczy X25519 używa algorytmu *drabinki Montgomery'ego*, który operuje tylko na współrzędnej $x$ punktu. Każdy krok algorytmu wykonuje stałą sekwencję operacji, jednego podwajania, jednego różnicowego dodawania, niezależnie od danych wejściowych, co zapewnia bezpieczeństwo przeciwko atakom _side channel_ @pornin_x25519.
- *Ed25519* bazuje na równoważnej krzywej skręconej Edwardsa o kompletnych wzorach dodawania (oznacza to, że działają one bez wyjątków dla dowolnie wybranych punktów na krzywej), wprowadza on również parametr losowy za pomocą SHA-512, aby uniknąć ryzyka wycieku klucza w przypadku słabego generatora liczb losowych (wada z ECDSA).

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

Nowoczesne protokoły takie jak TLS 1.3, WireGuard, QUIC, bazują integralność i poufność danych na dwóch metodach enkrypcji *AEAD*, *AES-GCM* i *ChaCha20-Poly1305*. Pomimo tego, że oba algorytmy realizują ten sam cel, to na prostych mikrokontrolerach bez sprzętowej akceleracji kryptograficznej *ChaCha20-Poly1305* jest bezpieczniejszy i szybszy niż *AES-GCM* @aes_gcm_iot_2019 @palii_chacha20_2026.

\ Największą wadą programowej implementacji AES są ataki czasowe (_timing attacks_). W celu przyspieszenia działania AES na procesorach 32-bitowych, operacje łączy się w duże tablice przeglądowe (_T-tables_). Ponieważ indeksy odczytów z tabeli zależą od klucza, minimalne wahania w czasie dostępu do pamięci mogą ujawnić wartość klucza @bernstein_cache_2005 (podatności w wolfSSL do wersji 5.6.5 na mikrokontrolerach Cortex-M). Zabezpieczanie programowe AES przed tego typu atakami wymaga skomplikowanych technik (_bitslicing_) które znacząco spowalniają kod. Szyfr *ChaCha20* w pełni eliminuje ten problem, jest to algorytm z rodziny ARX (_Addition-Rotation-XOR_) które wykorzystują tylko proste operacje arytmetyczno-logiczne: dodawanie, przesunięcie cykliczne oraz operację XOR. Nie korzysta on również z tablic przeglądowych, i nie posiada rozgałęzień, co sprawia, że czas wykonania *ChaCha20* jest stały (_constant-time_), zapewnia to odporność na ataki typu _side-channel_ bez spowalniania działania programu @palii_chacha20_2026.

\ Drugim powodem dlaczego *ChaCha20-Poly1305* jest lepszym wyborem na mikrokontrolerach jest dużo niższy koszt wykonania algorytmu. Funkcja sprawdzająca integralność w AES-GCM (GHASH) wymaga operacji w ciele $"GF"(2^128)$, proste mikrokontrolery nie posiadają akceleracji sprzętowej tych operacji, więc muszą je emulować co jest od dwóch do czterech razy wolniejsze niż szyfrowanie, i znacząco spowalnia układ, nawet w przypadku w którym mikrokontroler posiada akcelerację AES @aes_gcm_iot_2019.

\ Algorytm *Poly1305* bazuje na zwykłej arytmetyce liczb całkowitych (modulo $2^130 - 5$), są to podstawowe instrukcje w procesorach 32-bitowych. Dzięki temu na procesorach ARM bez akceleracji sprzętowej, *ChaCha20-Poly1305* działa do 3 razy szybciej niż AES-GCM. Z tego powodu standard TLS 1.3 zaleca *Poly1305* jako preferowany algorytm dla urządzeń bez sprzętowej akceleracji AES @palii_chacha20_2026.

\ Należy pamiętać, że żaden z tych algorytmów nie jest odporny na użycie tego samego wektora _nonce_ dwa razy, w przypadku losowej generacji wektora zalecane jest użycie *XChaCha20-Poly1305* gdzie _nonce_ ma rozmiar 192 bitów ze względu na to, że z 96-bitowy nonce algorytmu ChaCha20-Poly1305 posiada ryzyko kolizji ze względu na paradoks urodzin, wymagane jest tylko około $2^32"-"2^48$ wiadomości z losowo generowanym wektorem, aby wystąpiła kolizja @palii_chacha20_2026.

=== Problem wymiany kluczy bez serwera centralnego: TOFU i podpisy cyfrowe

Nawet bezbłędna implementacja *X25519* oraz *ChaCha20-Poly1305* nie chroni przed atakiem _Man-in-the-middle_ (MITM), przez brak możliwości powiązania klucza publicznego z tożsamością. Rozproszone sieci _off-grid_ z definicji nie mają możliwości użycia centralnego serwera uwierzytelniającego.

\ W rezultacie w niezależnych sieciach _off-grid_ stosuje się model *Trust On First Use (TOFU)* obecny między innymi w protokole SSH lub komunikatorach takich jak Signal. Model TOFU polega na zapisaniu klucza publicznego rozmówcy podczas pierwszego nawiązania połączenia, i od tego momentu domyślnie ufa mu, następnie przy każdej kolejnej interakcji, klucz jest sprawdzany, i w przypadku gdy się nie zgadza, użytkownik otrzymuje komunikat o potencjalnych ataku MITM. Pomimo tego, że model TOFU zapewnia wysoką użyteczność w sieciach zdecentralizowanych, istnieje w nim jedna krytyczna podatność, czyli podmiana klucza w trakcie pierwszej wymiany @sok_secure_messaging_2015.

\ W celu wyeliminowania tej podatności, stosuje się cztery uzupełniające się mechanizmy:
- *Odciski kluczy (fingerprints)* - skrót klucza publicznego, prezentowany za pomocą znaków lub kodu QR. Porównanie ich poprzez zeskanowanie twarzą w twarz, weryfikuje zaufanie.
- *Krótkie ciągi uwierzytelniające (SAS - Short Authentication Strings)* - metoda z protokołu ZRTP, gdzie obie strony odczytują na głos ciąg znaków wynikający z wcześniej uzgodnionego sekretu co łączy kryptografię z biometrią.
- *Podpisy cyfrowe* - długoterminowy klucz, który służy do podpisywania kluczy wymiany lub zaufanych urządzeń. Pozwala nam to ograniczyć liczby ręcznych weryfikacji odcisków w zespołach z wieloma urządzeniami. Sam podpis jednak gwarantuje jedynie ciągłość posiadania klucza, i nie zastępuje potwierdzenia tożsamości.
- *Wspólne hasła (PSK - Pre-Shared Key)* - Polega to na tym, że przed wdrożeniem członkowie zespołu ustalają poza pasmem wspólne hasło, za pomocą którego generowany jest klucz szyfrujący przy użyciu funkcji wyprowadzania kluczy (Argon2, KDF, HKDF). W nowoczesnych rozwiązaniach używa się również protokołów PAKE (Password-Authenticated Key Exchange), dzięki którym możemy uwierzytelnić wymianę kluczy asymetrycznych za pomocą krótkiego hasła nie ujawniając go w sieci. Jednym z minusów tego rozwiązania jest konieczność bezpiecznego rozpowszechnienia sekretu przed połączeniem oraz utrata poufności całej sieci w sytuacji w której jedno urządzenie zostanie przejęte, a klucz odczytany z pamięci @sok_secure_messaging_2015.

\ Ważnym wnioskiem z badań użyteczności zabezpieczeń (_"SoK: Secure Messaging"_ @sok_secure_messaging_2015 i _"Secure Messaging Authentication Ceremonies Are Broken"_ @vazquez_ceremonies_2020) jest to, że jeśli weryfikacja użytkowników ukryta jest w menu, to niemal nikt jej nie używa, i nie rozumie zagrożenia MITM. Dopiero w sytuacji gdy interfejs użytkownika jasno mówi, że kontakt jest niezweryfikowany, zrozumienie ceremonii weryfikacji wzrosło z 16% do 50%, a znaczenie zaufanych identyfikatorów z 27% do 56%, należy więc pamiętać, że przy projektowaniu urządzeń _off-grid_ mechanizm zaufania kontaktów powinien być jasnym i integralnym elementem parowania urządzeń @vazquez_ceremonies_2020.

= Wybrane algorytmy

== Kryteria doboru stosu technologicznego

Najważniejszą częścią doboru stosu technologicznego w systemach wbudowanych jest wzięcie pod uwagę ograniczeń które ograniczają projekt. W przypadku transmisji LoRa najważniejszym ograniczeniem jest rozmiar pojedynczego pakietu, który wynosi 255 bajtów oraz regulacje prawne, które mocno ograniczają czas nadawania i limity mocy. Sprawia to, że każdy bajt podczas transmisji jest jeszcze bardziej cenny niż w innych typach transmisji, dużym wyzwaniem jest więc uzyskanie silnej kryptografii względem zużycia baterii i czasu trwania transmisji (_Time on Air_). W poprzednim rozdziale przywołano parametry układu SX1276, ponieważ to dla tej rodziny układów Semtech opublikował dokument techniczny ze wzorami do obliczania parametrów LoRa, w samym  projekcie wykorzystano moduł SX1262 który posiada konfigurowalne parametry:
- Bandwidth w zakresie 7.81 - 500 kHz
- Spreading Factor w zakresie 5 - 12
- Kodowanie w zakresie 1 - 4
- Częstotliwość w zakresie 410 - 510 MHz
- Moc w zakresie -9 dBm - +22 dBm 

\ Parametry zostały wybrane tak, aby osiągnąć złoty środek, BW = 125 kHz, aby zachować czułość odbiornika na tanim układzie, SF = 9 w celu przyspieszenia transmisji, kodowanie 4/5 dla podstawowej korekcji błędów, standardową preambułę 8 symboli oraz moc 10 dBm ze względu na normy europejskie. 

\ Ze względu na brak sprzętowej akceleracji AES oraz ograniczoną pamięć SRAM, w projekcie zrezygnowano z bibliotek takich jak mbedTLS czy wolfSSL na rzecz lżejszej biblioteki *Monocypher*, napisana jest ona w standardzie C99. Dzięki temu wydajność kryptografii na rdzeniu ARM Cortex-M0+ jest dużo lepsza. Główne zalety *Monocypher* to:
- Odporność na _side-channel attacks_, wszystkie krytyczne operacje kryptograficzne zaimplementowane są w sposób stałoczasowy (_constant-time_), bez rozgałęzień programu, i indeksowania tablic na podstawie sekretów, co chroni program przed analizą czasową (_timing attacks_).
- Biblioteka implementuje wydajne algorytmy na architekturze 32-bitowej, algorytmy z rodziny ARX (_Addition-Rotation-XOR_) takie jak ChaCha20 oraz BLAKE2b, bazują one na prostych 32-bitowych operacjach arytmetyczno-logicznych, co zapewnia nieporównywalnie lepszą wydajność na procesorach Cortex-M0+ w stosunku do programowych implementacji AES.
- Brak dynamicznej alokacji pamięci (malloc oraz free) i operuje na buforach przekazanych przez program główny, które są alokowane statycznie lub na stosie. Całkowicie eliminuje to zjawisko wycieków pamięci przy długotrwałej pracy urządzenia.
- Łatwość integracji w projekcie zapewniana jest przez sposób w jaki biblioteka jest rozpowszechniana, czyli para dwóch plików źródłowych (monocypher.c i monocypher.h) bez innych zależności co zmniejsza rozmiar końcowego programu oraz upraszcza jego kompilację.

== Zastosowane algorytmy kryptograficzne

=== Generowanie kluczy oraz tożsamości urządzenia

Każde urządzenie identyfikowane jest na podstawie 8-bajtowego identyfikatora (`deviceId`) który pochodzi z fabrycznego identyfikatora pamięci Flash oraz na podstawie kryptograficznej tożsamości urządzenia (`DeviceIdentity`) która wyprowadzana jest z ziarna 256-bitowego (`seed`). W celu łatwiejszej pracy nad prototypem przy pierwszym uruchomieniu generuje on tymczasowe ziarno przy użyciu *BLAKE2b* z tekstu `"dev"` identyfikatora urządzenia oraz licznika czasu. Użytkownik może je zastąpić własnym 256-bitowym ziarnem co powoduje ustalenie nowej tożsamości urządzenia oraz wyczyszczenie listy kontaktów.

\ Użycie tego samego sekretu w wielu algorytmach może prowadzić do kompromitacji danych, dlatego zastosowano separację domenową (_domain separation_). Przy użyciu głównego ziarna `seed` i jawnych etykiet `"sign"` i `"box"` funkcja skrótu *BLAKE2b* wyprowadza dwa 32-bajtowe klucze. Jeden z nich używany jest w generatorze pary kluczy podpisu *Ed25519* która używana jest w procesie uwierzytelniania protokołu parowania. Drugi z nich to klucz prywatny używany w algorytmie uzgadniania kluczy *X25519*, na którego podstawie wyliczany jest 32-bajtowy klucz publiczny. 

\ Ważnym elementem tożsamości jest również 8-bajtowy odcisk (_fingerprint_) który wyliczany jest przy użyciu skrótu *BLAKE2b* na podstawie `deviceId` oraz obu kluczy publicznych, który jest widoczny w interfejsie graficznym, i pozwala użytkownikom na weryfikację tożsamości w modelu TOFU. Po zakończeniu generacji tożsamości bufory oraz stos są bezpiecznie czyszczone funkcją `crypto_wipe`, a tożsamość urządzenia jak i suma kontrolna CRC-32 zapisywane są w pamięci Flash urządzenia. Cały proces przedstawia następujący schemat:

#figure(
  image("images/diagram1.svg"),
  caption: [Schemat wyprowadzenia kluczy urządzenia (Ed25519 i X25519) oraz odcisku tożsamości (BLAKE2b)]
)

=== Generowanie klucza sesyjnego i szyfrowanie wiadomości

Bezpieczeństwo komunikacji pomiędzy dwoma sparowanymi urządzeniami zapewnia wykorzystanie 256-bitowego symetrycznego klucza sesyjnego, który wyliczany jest przy pomocy algorytmu *X25519* na podstawie własnego klucza prywatnego i klucza publicznego odbiorcy, który został zapisany w trakcie parowania urządzeń. Otrzymany sekret nie jest jednak wykorzystywany do szyfrowania, jest on natomiast użyty jako część wejścia do funkcji skrótu *BLAKE2b*, razem z kluczami publicznymi obu użytkowników posortowanych leksykograficznie. Zapewnia to silną entropię klucza, wiąże klucz z tożsamościami obu stron oraz sprawia, że kierunek transmisji nie ma wpływu na klucz sesyjny. Po zakończeniu szyfrowania i deszyfrowania wszystkie bufory kryptograficzne wykorzystane w tym procesie są bezpiecznie czyszczone za pomocą `crypto_wipe`.

#figure(
  image("images/diagram2.svg"),
  caption: [Schemat wyprowadzenia symetrycznego klucza sesyjnego]
)

\ Faktyczne szyfrowanie wiadomości odbywa się za pomocą algorytmu AEAD *XChaCha20-Poly1305* (`crypto_aead_lock`). Algorytm ten nie wymaga wektora inicjalizującego o wysokiej entropii, ściśle wymaga jednak jego unikalności, z tego powodu jest on budowany deterministycznie, 24-bajtowy _nonce_ składa się z identyfikatora nadawcy (`SenderID`, 8 B), następnie identyfikatora odbiorcy (`RecipientID`, 8 B), a na końcu z 32-bitowego indeksu wiadomości (`Sequence`, 4 B) i 4 bajtów zerowych. Zawarcie obu adresów rozróżnia kierunki transmisji, nawet jeśli indeks wiadomości się powtórzy, co eliminuje ryzyko kolizji.

\ Pakiet `EncryptedMessage` składa się z nagłówka `LCL1` (27 B), nazwy urządzenia (16 B), odcisku urządzenia (8 B), wektora inicjalizacji (24 B), znacznika MAC *Poly1305* (16 B) oraz zaszyfrowanej wiadomości (1-96 B). Pierwsze 75 bajtów pakietu stanowi AAD (_Additional Authenticated Data_), czyli uwierzytelnione dane dodatkowe które nie podlegają szyfrowaniu, ponieważ są niezbędne do poprawnej transmisji oraz odbioru pakietu (zawierają identyfikator nadawcy oraz _nonce_), ale za to są brane pod uwagę przy liczeniu tagu MAC. Dzięki AEAD z AAD otrzymujemy komunikację chronioną przed manipulacją bitami (_bit-flipping_), podmianą niezaszyfrowanych metadanych w nagłówku, fałszowaniem wiadomości (_spoofing_) oraz uszkodzeniami pakietu wynikającymi z zakłóceń sygnału lub szumu tła, które nie zostały wykryte przez CRC modułu LoRa.

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
  caption: [Schemat pakietu *EncryptedMessage*]
)

== Protokół parowania urządzeń

Aby nawiązać szyfrowaną komunikację najpierw wymagana jest wymiana kluczy publicznych dwóch urządzeń. Z powodu braku centralnego serwera który zarządzałby dystrybucją kluczy, zaimplementowany został dwukrokowy protokół parowania, składa się on z pakietów `InviteBroadcast` i `InviteAccept`. Zaproszenie (`WireInvite`, 144 B) składa się z nazwy użytkownika (16 B), klucza publicznego podpisu *Ed25519* (32 B), klucza publicznego wymiany *X25519* (32 B) oraz 64-bajtowego podpisu cyfrowego *Ed25519*. Podpis wyliczany jest na podstawie typu pakietu, identyfikatora urządzenia `deviceId`, nazwy urządzenia, i obu kluczy publicznych nadawcy, dzięki czemu otrzymujemy integralność danych w trakcie transmisji.

\ Protokół może działać w dwóch trybach pracy, otwartym lub chronionym wspólnym hasłem (PSK). W trybie otwartym `WireInvite` przesyłany jest jawnie, bez szyfrowania, a potwierdzenie tożsamości odbywa się poprzez weryfikację przez użytkownika 16-znakowego odcisku tożsamości urządzenia (_fingerprint_) który wyświetlany jest po odbiorze zaproszenia w interfejsie użytkownika. W trybie szyfrowania hasłem, z ciągu znaków podanego w interfejsie użytkownika wyprowadzany jest za pomocą *Argon2id* 256-bitowy klucz (bufor 16 KB, 2 przebiegi, 1 wątek). Cały ładunek `WireInvite` jest następnie zaszyfrowany za pomocą algorytmu *XChaCha20-Poly1305*, co przez dodanie 24-bajtowego wektora inicjalizującego i 16-bajtowego tagu MAC, zwiększa rozmiar ładunku do 184 B (cała ramka ma rozmiar 211 B). AAD obejmuje `WireHeader` (27 B) co chroni zaproszenie przed atakami typu manipulacji nagłówkiem (_header tampering_).

\ Parowanie zaczyna się od wysłania przez inicjatora (Urządzenie A) pakietu typu `InviteBroadcast` na adres `0xFF..FF`. Następnie urządzenie B (Odbiorca) odbiera ramkę, i w przypadku zaproszenia jawnego weryfikuje podpis *Ed25519*, jeśli zaproszenie jest zaszyfrowane hasłem to urządzenie przechowuje ramkę w pamięci oraz czeka na wprowadzenie i zatwierdzenie hasła (niepoprawne hasło odrzuca pakiet poprzez weryfikację *Poly1305*). Jeśli hasło jest poprawne to pakiet jest deszyfrowany, a system sprawdza czy kontakt nie koliduje z wcześniej zapisanym kontaktem, o innym kluczu publicznym, co chroni przed atakiem typu _spoofing_ (podszywanie się pod znanego rozmówcę). Po akceptacji przez użytkownika, urządzenie odbiorcy zapisuje kontakt do pamięci Flash (`ContactStore`), i wysyła do urządzenia A podpisany pakiet `InviteAccept` (w trybie jawnym pakiet nie jest szyfrowany, a w trybie szyfrowanym chroniony jest tym samym hasłem). Po odebraniu i weryfikacji pakietu, urządzenie A zapisuje kontakt w pamięci Flash, a parowanie kończy się sukcesem.

#figure(
  image("images/diagram3.svg"),
  caption: [Schemat protokołu parowania urządzeń]
)

== Protokół wymiany wiadomości oraz ACK

Sparowane urządzenia mogą wymieniać między sobą zaszyfrowane wiadomości tekstowe, o maksymalnej długości 96 bajtów (`MaxPlainTextSize`). Po zatwierdzeniu wiadomości w interfejsie użytkownika, warstwa sesji radia (`RadioSession`) sprawdza czy wybrany jest kontakt oraz czy moduł radiowy LoRa nie jest w trakcie transmisji, po czym wywołuje `Packet::encodeMessage`. Jeśli ramka zostanie poprawnie przyjęta przez sterownik układu SX1262, to wiadomość zapisywana jest w lokalnej historii chatu (`ChatHistory`), a jej status ustawiany jest na `Sending`, równocześnie w tle uruchamiany jest licznik czasu oczekiwania na potwierdzenie odebrania wiadomości ACK (`AckTimeoutMs = 3000 ms`), powiązany z indeksem sekwencyjnym `sequence`.

\ Odbiorca nasłuchuje na łączu i w przypadku wykrycia ramki jest ona wstępnie walidowana w `Packet::decode` (sprawdzenie znacznika magic `LCL1`, wersji protokołu oraz długości zadeklarowanego ładunku z faktycznym jego stanem), a następnie jest filtrowana na bazie adresu (`Packet::isForDevice`), urządzenie odrzuca ramki pochodzące z własnego `deviceId`. Jeśli pakiet jest typu `EncryptedMessage` to program wyszukuje nadawcę pakietu w bazie zapisanych kontaktów `ContactStore`, odrzucając tym samym ramki do których nie mamy zapisanego kontaktu, następnie weryfikuje zgodność odcisku tożsamości i wykonuje deszyfrację AEAD. Jeśli wiadomość została poprawnie uwierzytelniona trafia ona do historii chatu, wraz z odczytanymi parametrami transmisji (`RSSI` i `SNR`).

\ Po odszyfrowaniu pakietu i zapisaniu wiadomości w historii, odbiorca wysyła wiadomość zwrotną w postaci `MessageAck` (35 B: 27-bajtowy nagłówek `WireHeader` i 8-bajtowy ładunek `WireAck` który zawiera wartości `RSSI` oraz `SNR`). Nadawca odbiera pakiet `MessageAck`, sprawdza czy pasuje on do oczekiwanego numeru `awaitingAckSeq_`, zmienia status wysłanej wiadomości na `Delivered` oraz uzupełnia zwrócone przez odbiorcę wartości `RSSI` i `SNR`. Jeśli pakiet `MessageAck` nie pojawi się w limicie czasu 3000 ms, program oznacza wiadomość jako `Failed`, i wyświetla w interfejsie użytkownika komunikat o przekroczeniu czasu oczekiwania na odpowiedź. 

#figure(
  image("images/diagram4.svg"),
  caption: [Schemat protokołu wymiany wiadomości]
)

== Algorytm transferu plików

RP2040 nie posiada wbudowanego systemu plików, z tego względu transfer plików odbywa się we współpracy aplikacji desktopowej z urządzeniem. Urządzenie pełni rolę nadawczą oraz szyfrującą, podczas gdy komputer dostarcza faktyczne dane do przesłania. W momencie w którym użytkownik wybierze plik który chce przesłać w interfejsie aplikacji desktopowej, do urządzenia wysyłany jest komunikat `FileExchangeStart`, który posiada nazwę pliku (do 64 B) oraz jego rozmiar. Moduł `FileTransfer` generuje jednorazowy, 256-bitowy klucz pliku (`currentFileKey_`) za pomocą funkcji skrótu *BLAKE2b* który na wejściu otrzymuje identyfikator urządzenia `deviceId`, czas od uruchomienia urządzenia oraz indeks sekwencyjny wiadomości. Wygenerowany klucz wraz z metadanymi pliku jest szyfrowany za pomocą algorytmu AEAD *XChaCha20-Poly1305* przy użyciu wcześniej wygenerowanego klucza sesyjnego kontaktu, a następnie tak zaszyfrowany fragment nadawany jest w pakiecie `FileExchangeStart`.

\ Aplikacja desktopowa dzieli wybrany plik na fragmenty o rozmiarze 128 bajtów. Każdy z nich jest szyfrowany na mikrokontrolerze przy pomocy algorytmu *ChaCha20-IETF* przy użyciu jednorazowego klucza pliku i 12-bajtowego wektora inicjalizującego wyprowadzonego na podstawie 32-bitowego indeksu fragmentu (`chunkIndex`) uzupełnionego zerami. Tak zaszyfrowany fragment trafia do pakietu typu `FileExchangeData` (159 B jeśli fragment ma 128 bajtów), dzięki czemu bezpiecznie mieści się w 255-bajtowym limicie pakietów LoRa. W przeciwieństwie do szyfrowanych E2EE wiadomości tekstowych, pojedyncze pakiety zawierające fragmenty pliku nie przenoszą 16-bajtowego tagu MAC, i nie korzystają z potwierdzeń otrzymania (ACK), dzięki czemu transmisja trwa krócej, i skraca czas zajęcia kanału (_Time on Air_).

\ W celu mitygacji przepełnienia buforów urządzenia przez magistralę USB, zastosowano mechanizm kontrolowania przepływu (_flow control_). Oznacza to, że kolejny fragment pliku czeka `awaitingTxAck_` dopóki układ SX1262 wyśle poprzedni fragment `TxDone`. Dopiero po tym mikrokontroler informuje komputer o tym, że potrzebuje kolejny fragment, poprzez wysłanie ramki `FileExchangeAck`. Po wysłaniu wszystkich fragmentów nadawca wysyła pakiet typu `FileExchangeEnd`, sygnalizując koniec transmisji pliku. W każdej chwili transferu obie ze stron mogą go również anulować, poprzez ramkę `FileExchangeCancel`, czyszcząc tym samym stan transferu i klucz sesji.

#figure(
  image("images/diagram5.png"),
  caption: [Schemat protokołu wymiany plików]
)

= Implementacja

== Architektura sprzętowa

Bazą urządzenia jest płytka Waveshare RP2040-LoRa, która posiada dwurdzeniowy mikrokontroler RP2040, taktowanie do 133 MHz, 264 KB pamięci SRAM oraz 2 MB pamięci Flash. Dodatkowo wyposażona jest ona w układ nadawczo-odbiorczy (_transceiver_) Semtech SX1262 z oscylatorem kompensowanym temperaturowo. Układ RP2040 jest lepszy od konkurencji, ponieważ nie posiada energochłonnych modułów Wi-Fi lub BLE, dzięki czemu urządzenie może pracować na baterii dłużej, dużą zaletą jest również dojrzałość Pico SDK, jak i niski całkowity koszt prototypu urządzenia.

\ Urządzenie komunikuje się z peryferiami dzięki trzem niezależnym magistralom mikrokontrolera:
- *SPI0 (30 MHz)* steruje on wyświetlaczem TFT LCD ILI9341 320x240 (orientacja pozioma, `MADCTL = 0xA0`)
- *SPI1 (10 MHz)* obsługuje moduł LoRa SX1262 oraz linie sterujące `RESET`, `BUSY`, `DIO1` które obsługują przerwania oraz `ANT_SWITCH_POWER` do zmiany toru antenowego RX/TX
- *I2C1 (100 kHz)* obsługuje miniaturową klawiaturę QWERTY M5Stack CardKB pod adresem `0x5F`.

\ Tożsamość urządzenia (`DeviceIdentity`) jak i baza kontaktów jest trwale przechowywana oraz zarządzana przez klasę `ContactStore`. Zapisuje ona dane w ostatnim 4-kilobajtowym sektorze pamięci Flash, z nagłówkiem `LCL2`, wersją oraz sumą kontrolną CRC-32. Płytka ta współdzieli magistralę przez kod programu i pamięć Flash, z tego powodu operacje usuwania i zapisywania sektora (`flash_range_erase` i `flash_range_program`) wykonują się atomowo, i tymczasowo wyłączają przerwania. Schemat połączeń oraz pinów widoczny jest na poniższym diagramie.

#figure(
  image("images/diagram6.svg"),
  caption: [Schemat połączeń]
)

== Środowisko programistyczne

Oprogramowanie układu zostało napisane w języku C++ (logika, protokoły, interfejs) a biblioteki zewnętrzne w C (sterowniki układów, kryptografia). Proces budowania wykorzystuje *CMake* z generatorem *Ninja*, *Raspberry Pi Pico SDK 2.1.1*, kompilator *ARM GNU Toolchain* i narzędzie *picotool 2.1.1* do programowania urządzenia przez USB. Konfiguracja wymusza modulację LoRa (`USE_MODEM_LORA`) oraz normę europejską (`REGION_EU433`), a standardowe wejście/wyjście przekierowane jest na interfejs USB CDC (`pico_enable_stdio_usb`).

\ Projekt korzysta z trzech zewnętrznych bibliotek których kod znajduje sie w katalogu `lib/`:
- *font8x8* to lekka czcionka bitmapowa 8x8 pikseli, używana do wyświetlania tekstu na ekranie.
- *LoRaMac-node* to oficjalna biblioteka Semtech dostarczająca warstwę HAL (_Hardware Abstraction Layer_) oraz sterownik układu SX1262, która obsługuje moduł LoRa poprzez adaptery sprzętowe w `src/board/`.
- *Monocypher* to biblioteka kryptograficzna rozpowszechniana w stylu _single-header_, która działa stałoczasowo i bez dynamicznej alokacji pamięci.

\ Zaimplementowano również aplikację terminalową (TUI) na komputer, została ona napisana w języku *Go*, w katalogu `desktop_ui`. Do budowy interfejsu wykorzystano bibliotekę *Bubble Tea* oraz *Lip Gloss*, a do komunikacji przez wirtualny port szeregowy bibliotekę `go.bug.st/serial`. Okno dialogowe przy wyborze pliku obsługuje biblioteka `sqweek/dialog`.

== Opis maszyn stanów oraz przepływu danych

Główna klasa `App` wykonuje nieblokującą pętlę co 20 ms (`App::run`) oraz zarządza czteroma głównymi podsystemami:
- `HostLink` to łącze USB do komunikacji z aplikacją desktopową.
- `RadioSession` zarządza sesją LoRa.
- `FileTransfer` odpowiedzialna jest za koordynację i obsługę transferu plików.
- `UiController` obsługuje interakcje użytkownika z interfejsem graficznym.

\ Podsystemy komunikują się ze sobą poprzez polecenia `UiCommand`, flagi odświeżania (`dirty_`) oraz jednokierunkowy kanał powiadomień `NoticeSink` co zmniejsza złożoność kodu.

#figure(
  image("images/diagram7.svg"),
  caption: [Diagram stanu UI]
)

Maszyna stanów interfejsu użytkownika (`UiController`) odpowiedzialna jest za zarządzanie czterema ekranami interfejsu:
- *Chat* wyświetla historię rozmowy oraz umożliwia wysyłanie wiadomości
- *Contacts* lista sparowanych urządzeń
- *Invite* ekran odpowiedzialny za wysyłanie oraz akceptowanie żądań parowania urządzeń
- *Identity* podgląd odcisku tożsamości urządzenia, zmiana nazwy urządzenia oraz zmiana ziarna kryptograficznego

\ Jeśli rozpocznie się transfer pliku, to urządzenie wyświetla nakładkę pokazującą postęp transferu (`filePopupWasActive_`), jak i przycisk umożliwiający przerwanie transmisji.

#figure(
  image("images/diagram8.svg"),
  caption: [Diagram stanu modułu radiowego]
)

Po inicjalizacji urządzenia i modułu LoRa, radio przechodzi w stan spoczynkowy *Idle*, w którym nasłuchuje i odbiera przychodzące ramki. Z tego stanu urządzenie przechodzi przez 3 inne stany, po których wraca do stanu spoczynku *Idle*:
- Oczekiwanie na potwierdzenie odbioru wiadomości (*AwaitingAck*), po wysłaniu wiadomości moduł przechodzi do stanu *AwaitingAck*, a po otrzymaniu potwierdzenia odbioru *MessageAck* z poprawnym indeksem sekwencyjnym (*Sequence OK*) wiadomość przechodzi do stanu *Delivered* a moduł do stanu `Idle`, a w przypadku braku otrzymania potwierdzenia w limicie czasu `AckTimeoutMs` (*Timeout*) wiadomość przechodzi do stanu *Failed*, a moduł do stanu `Idle`.
- Nadawanie pliku (*SendingFile*) rozpoczyna się poprzez otrzymanie komendy *FileExchangeStart* po interfejsie USB CDC, i tym samym przełącza moduł do stanu *SendingFile*. W tym stanie urządzenie odbiera fragmenty pliku przesyłane z aplikacji desktopowej i transmituje je przez moduł LoRa aż do zakończenia pliku (*FileExchangeEnd*) lub anulowania transferu (*FileExchangeCancel*).
- Odbiór pliku (*ReceivingFile*) rozpoczyna się po odebraniu pakietu *FileExchangeStart* po kanale LoRa, i przełącza urządzenie w stan *ReceivingFile*, w którym urządzenie odbiera dane przesyłane przez nadawcę aż do wystąpienia pakietu *FileExchangeEnd* lub *FileExchangeCancel*.

== Interfejsy użytkownika

Warstwa interfejsu użytkownika została wydzielona od konkretnego sprzętu za pomocą warstwy abstrakcji (`ui::screenModel`). Widoki budowane są za pomocą widżetów (`HeaderBar`, `TextBox`, `Divider`, `ListItem`, `Label`, `Button`, `StatusBar`), a widoki są porównywane (poprzedni i bieżący stan `ScreenModel::sameContent`), aby uniknąć niepotrzebnego odświeżania magistrali SPI oraz migotania ekranu.

\ Podczas inicjalizacji urządzenia (`UiController::init`) program sprawdza czy klawiatura I2C jest podłączona i zależnie od tego posiada dwa tryby:
- *Tryb autonomiczny* w przypadku w którym klawiatura jest obecna, głównym ekranem zostaje wyświetlacz LCD (`LcdScreen`), a metodą wprowadzania danych klawiatura CardKB (`CardKbInput`). Jednocześnie również jeśli urządzenie wykryje podłączony komputer z aplikacją desktopową to zmiany ekranu są wysyłane i wyświetlane w aplikacji desktopowej.
- *Tryb bez CardKB*, urządzenie przełącza wejście i wyjście na interfejs UART (`UartScreen` oraz `UartInput`) co umożliwia pełną obsługę urządzenia przy użyciu komputera.

\ Urządzenie komunikuje się z komputerem poprzez USB CDC, za pomocą autorskiego protokołu (`proto::Framing`). Każdy pakiet zaczyna się od dwóch bajtów `0xA5 0x5A`, typu wiadomości (`MessageType`), 16-bitowej długości danych (LE, MaxPayload = 1536 B), danych oraz sumy kontrolnej `CRC-16`. Parser `FrameReader` który czyta dane bajt po bajcie, jeśli urządzenie zostanie podłączone do aplikacji desktopowej w trakcie działania, to przy najbliższej poprawnej ramce synchronizuje się i wymusza pełne odświeżenie ekranu (`HostLink::consumeResync`). Protokół ten przewiduje komunikaty (`Hello`, `Ping`, `Pong`), ekrany (`Screen`, `Notice`), dane klawiatury (`Key`) oraz ramki transferu plików (`FileExchangeStart`, `FileExchangeData`, `FileExchangeAck`, `FileExchangeEnd`, `FileExchangeCancel`).

\ Aplikacja interfejsu na PC (TUI, `desktop_ui`) automatycznie wykrywa podłączone urządzenia (`--auto`, `--list-ports`, `--port`) poprzez sprawdzanie dostępnych portów szeregowych pakietem `Ping`. W momencie nawiązania połączenia aplikacja renderuje w terminalu interfejs graficzny na podstawie uniwersalnych widżetów i stanu interfejsu z RP2040, przekazuje do urządzenia wejście z komputera,  oraz możliwość otwarcia dialogu wyboru pliku w celu jego transferu.

#figure(
  image("images/tui.png"),
  caption: [Widok aplikacji desktopowej TUI]
)

= Testy i walidacja

= Wnioski

== Podsumowanie realizacji projektu

Efektem niniejszej pracy jest zaprojektowane, zbudowane i działające, przenośne urządzenie do tekstowej komunikacji oraz przesyłania plików, działające w pełni autonomicznie, bez dostępu do infrastruktury publicznej (_off-grid_) w paśmie 433 MHz. Dzięki wykorzystaniu tanich komponentów takich jak RP2040, moduł LoRa SX1262, wyświetlacz ILI9341 oraz klawiatura CardKB, cały prototyp pojedynczego urządzenia mieści się w budżecie 150 zł.

\ Implementacja software'u potwierdziła ogromną przydatność biblioteki *Monocypher* w systemach wbudowanych. Użyte algorytmy z rodziny ARX (*XChaCha20-Poly1305*, *BLAKE2b*) oraz krzywa *Curve25519* (*X25519* i *Ed25519*) zapewniają bezpieczne szyfrowanie _End-to-End_ i uwierzytelnianie metadanych wiadomości których narzut jest pomijalny w porównaniu do czasu trwania transmisji pakietu LoRa (_Time on Air_). Wykorzystanie modelu abstrakcji interfejsu graficznego oraz protokołu USB CDC umożliwia jednoczesną kontrolę urządzenia poprzez wbudowany wyświetlacz oraz klawiaturę, jak i aplikację desktopową z rozszerzoną funkcjonalnością.

== Ograniczenia użytych zabezpieczeń i konstrukcji

Prototyp posiada kilka istotnych ograniczeń między innymi:

+ *Ograniczenia pasma 433 MHz:* Głównym problemem jest niska przepustowość modulacji LoRa, i limit czasu dostępu do widma (_duty cycle_ <= 10% przy mocy +10 dBm), w efekcie transfer dużych plików trwa długo, i wykorzystuje limit czasu transmisji, a zasięg urządzenia w gęstych miastach lub wewnątrz pojazdów znacząco maleje.
+ *Brak bezpiecznego magazynu kluczy:* RP2040 nie ma wbudowanego modułu bezpiecznego elementu (_Secure Element_), nie szyfruje również pamięci Flash przez co przy fizycznym dostępie do pamięci Flash klucze urządzenia mogą zostać odczytane.
+ *Brak zapisu licznika sekwencji do pamięci trwałej:* indeks wiadomości przechowywany jest tylko w pamięci RAM, i po utracie zasilania startuje od wartości początkowej. Stwarza to ryzyko powtórzenia wektora inicjalizującego pomiędzy sesjami.
+ *Prosty protokół wymiany plików:* pakiety `FileExchangeData` szyfrowane są przy użyciu algorytmu *ChaCha20-IETF* który nie posiada tagu uwierzytelniania MAC, więc jedyna ochrona błędów to 16-bitowe CRC modułu SX1262, a pakiety zarządzające protokołem (`FileExchangeEnd`, `FileExchangeCancel`) nie są zabezpieczone kryptograficznie. Brakuje również zliczania zgubionych pakietów w celu retransmisji, a odbiorca nie posiada limitu czasu bezczynności w przypadku rozłączenia nadawcy.
+ *Słaba entropia klucza transferu pliku:* klucz transferu pliku (`currentFileKey_`) wyprowadzany jest przy użyciu funkcji skrótu *BLAKE2b* z danych jawnych (`deviceId`, `sequence_`) oraz timestamp'u, ponieważ RP2040 nie posiada sprzętowego generatora liczb losowych który gwarantuje bezpieczeństwo kryptografii (TRNG).

== Kierunki dalszego rozwoju

Projekt można dalej rozwijać w następujący sposób:
- *Zmiana platformy na RP2350:* jest to nowszy mikrokontroler z procesorem ARM Cortex-M33, posiada również sprzętowe mechanizmy izolacji *ARM TrustZone*, sprzętowy generator liczb losowych (TRNG) oraz bezpieczny rozruch (*Secure Boot*), co rozwiązuje problem przechowywania kluczy i generowania entropii.
- *Ulepszenie niezawodności protokołu:* zapis licznika `sequence_` do pamięci nieulotnej, wprowadzenie retransmisji do transferu plików oraz tagów uwierzytelnienia MAC dla pakietów fragmentów plików, a także zaimplementowanie kompresji bezstratnej, w celu skrócenia czasu transmisji (_Time on Air_).
- *Ulepszenie warstwy sieciowej protokołu:* dodanie mechanizmu przekazywania pakietów (_mesh / multi-hop_) w celu zwiększenia zasięgu urządzenia w gęstych sieciach urządzeń, jednocześnie biorąc pod uwagę budżet _duty cycle_.

#pagebreak()

#bibliography("biblio/bibliografia.bib", title: "Bibliografia", style: "ieee")