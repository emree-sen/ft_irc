# TESTS — Manuel ve Otomasyon Planı

Bu dosya, değerlendirmenin manuel test akışını ve hızlı komut/script örneklerini içerir.

## 1) Bağlantı ve Kimlik Doğrulama

- Doğru parola:
```bash
nc -C 127.0.0.1 6667 <<'EOF'
PASS secret
NICK alice
USER alice 0 * :Alice
EOF
```
- Yanlış parola: `PASS wrong` → `464 Password incorrect`
- `NICK` çakışması: İkinci istemcide aynı nick → `433 Nickname is already in use`

## 2) Parçalı Veri (Zorunlu)

```bash
# Parça parça komut gönderimi
/home/emsen/Masaüstü/ft_irc/tests/scripts/partial_nc.sh 6667
```

## 3) Çoklu İstemci Yayın Testi

- 3 terminal açın; hepsi `JOIN #test` yapsın.
- Birinden `PRIVMSG #test :hello` gönderin; diğerlerinde mesajı görün.

## 4) Kanal Modları ve Yetkiler

- `+k` yanlış/eksik key → `475 Cannot join channel (+k)`
- `+l` aşımı → `471 Channel is full`
- `+i` altında davetsiz giriş → `473 Cannot join channel (+i)`
- `+t` iken op olmayan `TOPIC` → `482 You're not channel operator`
- `+o/-o` ile op ver/al ve `KICK` yetkisini doğrulayın.

## 5) Ayrılma/Kapanış

- `PART` ve `QUIT` ile bağlantının temiz kapanması; kanal üyeliklerinin ve poll kayıtlarının güncellenmesi.

## 6) Hatalı Paketler

- Eksik `\r\n`: Komut işlenmez, buffer’da bekler.
- Çok uzun satırlar: Mantıklı sınırlarla yönetilir.
- Bilinmeyen komut: `421 Unknown command` numeriği.

## 7) IPv4/IPv6

- IPv4: `127.0.0.1 6667`
- IPv6: `[::1] 6667` (dual-stack uygunsa)

---
Not: Bu plan zorunlu kısmı hedefler; bonuslar ancak zorunlu kısım kusursuzsa değerlendirilir.
# TESTS

Quick manual tests

1) Build and run
- make && ./ircserv 6667 secret

2) Register
- nc -C 127.0.0.1 6667
- PASS secret
- NICK alice
- USER alice 0 * :Alice

3) Partial data (nc -C):
- type: `com` then Ctrl-D, `man` then Ctrl-D, `d` then Enter
- Should be parsed as single command "command"

4) Multi-clients broadcast
- Connect alice, bob, carol; JOIN #t
- alice: PRIVMSG #t :hi
- bob and carol receive the message

5) Modes
- First joiner is op; set +i/+t/+k/+l; test INVITE, TOPIC restrictions

6) KICK/INVITE/TOPIC
- Non-op should be rejected; op succeeds

7) QUIT/PART
- Cleanup and broadcast
