# ft_irc Kullanım Kılavuzu (C++98 IRC Sunucusu)

Bu belge, ft_irc sunucusunu nasıl derleyip çalıştıracağınızı ve hem basit araçlarla (nc) hem de KVIrc gibi bir IRC istemcisiyle nasıl bağlanacağınızı adım adım anlatır. Komut örnekleri, kanal modları ve sorun giderme ipuçları dahildir.

## 1) Hızlı Başlangıç

- Derleme (Linux):

```bash
make
```

- Çalıştırma:

```bash
./ircserv <port> <password>
# örn.
./ircserv 6667 secret
```

- Sunucu açıldığında tüm soketler non-blocking ve tek bir poll() döngüsüyle yönetilir. IPv6 (mümkünse dual-stack) ve IPv4 desteklidir.

## 2) Bağlanma Yöntemleri

### 2.1) Netcat (nc) ile hızlı test

- CRLF gönderebilmek için `-C` bayrağını kullanın.

```bash
nc -C 127.0.0.1 6667
PASS secret
NICK alice
USER alice 0 * :Alice
JOIN #test
PRIVMSG #test :merhaba dünya
```

- Parçalı veri testi (zorunlu senaryo):

```bash
nc -C 127.0.0.1 6667
com^Dman^Dd
# ^D terminalde “end of file” (Ctrl-D) anlamında; burada satırı
# “com” + “man” + “d\n” olarak parça parça gönderirsiniz.
# Sunucu satırları CRLF ile çerçevelediği için parçaları birleştirir.
```

### 2.2) KVIrc ile bağlantı

KVIrc’i dağıtımınızın paket yöneticisinden kurun (ör. Debian/Ubuntu: `sudo apt install kvirc`).

KVIrc’te bağlanmanın iki pratik yolu vardır:

- Hızlı komutla bağlanma (önerilir):

```text
/server -p=6667 -w=secret 127.0.0.1
```

Açıklama:
- `-p=6667`: Port (örnek 6667)
- `-w=secret`: Sunucu parolası (IRC PASS). KVIrc bu değeri bağlanır bağlanmaz PASS olarak gönderir.

- GUI ile ağ/sunucu tanımı:
  1. Settings (Ayarlar) → Configure Servers (Sunucuları Yapılandır)
  2. “Add” (Ekle) ile yeni sunucu: Host: `127.0.0.1`, Port: `6667`
  3. Sunucu parolasını “Server password” alanına `secret` olarak girin
  4. Identity (Kimlik) sekmesinde takma ad (Nick) seçin
  5. Kaydedip "Connect" ile bağlanın

Notlar:
- Eğer parola alanını doldurmadıysanız ya da özel bir akış istiyorsanız, bağlandıktan sonra “Raw” komut gönderebilirsiniz:
  - `/quote PASS secret`
  - `/nick alice`
  - `/quote USER alice 0 * :Alice`
- PASS/NICK/USER tamamlandığında hoş geldiniz mesajı (001) dönecektir.

## 3) Temel IRC Komutları

Aşağıdaki komutları KVIrc kanal/mesaj giriş alanından veya nc üzerinden ham olarak gönderebilirsiniz.

- Kimlik & oturum:
  - `PASS <password>`
  - `NICK <nick>`
  - `USER <username> 0 * :<realname>`
  - `PING :token` → `PONG :token`
  - `QUIT :mesaj`

- Kanallar:
  - `JOIN #kanal` veya `JOIN #kanal key`
  - `PART #kanal :isteğe-bağlı-ayrılma-mesajı`
  - `PRIVMSG #kanal :metin` (kanaldaki diğer tüm üyelere yayınlanır)
  - `NOTICE #kanal :metin` (opsiyonel, bildirim)

- Özel mesaj:
  - `PRIVMSG nick :metin`
  - `NOTICE nick :metin`

- Kanal başlığı (topic):
  - `TOPIC #kanal` (görüntüle)
  - `TOPIC #kanal :yeni konu` (değiştir; `+t` modunda yalnızca op)

## 4) Operatör (op) ve Kanal Modları

- İlk katılan üye kanal operatörü (op) olur.
- Modlar ve örnekler:
  - Invite-only: `MODE #kanal +i` / `MODE #kanal -i`
  - Topic restricted: `MODE #kanal +t` / `MODE #kanal -t`
  - Key (parola): `MODE #kanal +k <key>` / `MODE #kanal -k`
  - Limit: `MODE #kanal +l <n>` / `MODE #kanal -l`
  - Op ver/al: `MODE #kanal +o <nick>` / `MODE #kanal -o <nick>`

- Davet ve atma:
  - `INVITE <nick> #kanal` (yalnızca op)
  - `KICK #kanal <nick> :sebep` (yalnızca op)

- Davetli kullanıcı `JOIN` ile içeri alınır (kanal `+i` ise davet gerekir).

## 5) Örnek Akışlar

- Üç kullanıcı aynı kanalda yayın:
  1. alice, bob, carol bağlanır ve `JOIN #test` der.
  2. alice `PRIVMSG #test :selam` gönderir.
  3. bob ve carol bu mesajı alır (alice hariç herkese yayınlanır).

- Topic sınırlı (`+t`) kanal:
  1. Op: `MODE #test +t`
  2. Op olmayan `TOPIC #test :yeni konu` dener → reddedilir.
  3. Op: `TOPIC #test :yeni konu` → kabul edilir.

- Kanal parolası (`+k`) ve limit (`+l`):
  - `MODE #test +k s3cr3t`
  - `MODE #test +l 2` → iki üye dolunca üçüncü girişe `Channel is full` döner.

## 6) Hatalar ve Yanıtlar (Örnekler)

- Yanlış parola: `:ircserv 464 :Password incorrect`
- Kayıt tamamlanmamış: `:ircserv 451 :You have not registered`
- Kanal yok veya üye değilsin: `:ircserv 442 #kanal :You're not on that channel`
- Op değilsin: `:ircserv 482 #kanal :You're not channel operator`
- Davet gerekli: `:ircserv 473 #kanal :Cannot join channel (+i)`
- Yanlış key: `:ircserv 475 #kanal :Cannot join channel (+k)`

Not: Numeric kodlar minimal ölçüde uygulanmıştır; uyumluluk için yeterlidir.

## 7) Sorun Giderme

- Bağlanamıyorum:
  - Sunucu gerçekten çalışıyor mu? Doğru port/parola mı?
  - Güvenlik duvarı/iptables yerel trafiği engelliyor olabilir.
- KVIrc PASS göndermiyor gibi:
  - `/server -w=secret -p=6667 127.0.0.1` komutunu kullanın veya Server Password alanını doldurun.
  - Gerekirse `/quote PASS secret` ile ham PASS gönderin ve ardından `/nick`, `/quote USER ...` girin.
- Parçalı veri testi başarısız:
  - `-C` bayrağıyla nc kullanın ve satırların `\r\n` ile bittiğinden emin olun.

## 8) Proje Yapısı ve Derleme Bayrakları

- C++98, `-Wall -Wextra -Werror -std=c++98 -pedantic`
- Dış kütüphane yok.
- Tek `poll()` döngüsü, tüm FD’ler `O_NONBLOCK`.

Klasörler:
- `src/`: Sunucu, poller, client, channel, parser, router, komutlar ve yardımcılar
- `tests/`: Manuel test notları ve scriptler
- `config/`: Örnek konfig (opsiyonel)

## 9) Gelişmiş İpuçları

- KVIrc’de otomasyon: Sunucu tanımına “on connect” komutları ekleyerek `JOIN #kanal` gibi işlemleri otomatikleştirebilirsiniz.
- Keepalive: `PING` atıp `PONG` yanıtını doğrulayabilirsiniz.
- Kaynak sınırları: Çok uzun satırlar mantıklı bir sınırla ele alınır; CRLF zorunludur.

---
Bu kılavuz, ft_irc’nin değerlendirme kriterlerine uygun, pratik kullanım senaryolarını kapsayacak şekilde hazırlanmıştır. Takıldığınız noktada test scriptlerini ve manuel test adımlarını inceleyin.

## 10) Kullanım

Aşağıda ft_irc sunucusuna KVIrc ile bağlanıp konuşma yapmak için adım adım bir senaryo bulabilirsin. Her adımda neyi, nasıl ve hangi sırayla yapman gerektiği açıkça belirtilmiştir.

1. Sunucuyu Derle ve Başlat
Terminalde proje klasörüne gir:

Sunucu arka planda çalışmaya başlar.

2. KVIrc’i Aç ve Sunucuya Bağlan
KVIrc’i başlat. Ana pencerede aşağıdaki adımları uygula:

Hızlı Bağlantı Komutu
KVIrc’in altındaki komut satırına şunu yaz:

Bu komut:

6667 portundan
“secret” parolası ile
localhost’a bağlanır.
Alternatif: Sunucu Tanımı ile Bağlanma
Menüden “Settings” → “Configure Servers” seç.
“Add” ile yeni sunucu ekle:
Host: 127.0.0.1
Port: 6667
Server password: secret
Nick/kimlik ayarlarını yap.
“Connect” ile bağlan.
3. Kimlik Doğrulama ve Kayıt
KVIrc, yukarıdaki komutla bağlanınca otomatik olarak PASS/NICK/USER komutlarını gönderir. Eğer nick veya kimlik ayarı eksikse, komut satırından elle gönderebilirsin:

Sunucu “Welcome” mesajı (001) dönerse kayıt tamamlanmıştır.

4. Kanal Aç ve Mesajlaş
Komut satırına:

Kanal açılır ve katılırsın. Şimdi mesaj gönderebilirsin:

Kanalda başka bir istemci varsa (örneğin başka bir KVIrc veya nc ile bağlanan), bu mesajı onlar da görür.

5. Ekstra: Kanal Modları ve Yetkiler
Kanalda operatör (op) isen, aşağıdaki komutları deneyebilirsin:

6. Ayrılma ve Kapatma
Kanalı terk etmek için:

Sunucudan çıkmak için:

Özet Akış
Sunucuyu başlat (./ircserv 6667 secret)
KVIrc ile bağlan (/server -p=6667 -w=secret 127.0.0.1)
Gerekirse PASS/NICK/USER komutlarını gönder
Kanala katıl (/join #test)
Mesaj gönder (/msg #test ...)
Kanal modlarını ve yetkileri dene (isteğe bağlı)
Ayrıl (/part) veya çık (/quit)
Her adımda bir hata alırsan, USAGE.md ve TESTS.md dosyalarındaki hata yanıtlarına ve ipuçlarına bakabilirsin. İstersen başka bir istemciyle (ör. nc) aynı anda bağlanıp çoklu istemci yayınını da test edebilirsin.