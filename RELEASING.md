# RELEASING.md — bir sürümü yayınlama kılavuzu

Bu dosya, ccharts'i yayınlamak için gereken tüm operasyon bilgisini toplar.
Hem insanlar hem de ajanlar (LLM) burada anlatılanları takip etmelidir.
Bu bilginin çoğu, `v0.2.1` yayını sırasında ampirik olarak edinildi; aynı
hataların tekrarlanmaması için buraya yazılmıştır.

---

## 0. Genel çalışma prensibi

- **Her zaman "madde madde" git.** Tüm registry'leri tek adımda "hepsi birden"
  açıp çalışmayanı canlıya taşıma. Önce tek tek doğrula, emin olunca birleştir.
- Bir sürümü tek `v<version>` tag'i yayınlatır. Tag push ettiğinde
  `.github/workflows/publish.yml` tüm registry'leri açmaya çalışır.
- Bu repo'nun kullanıcısı tek-tag-kapsamlı "hepsi birden" target sürümü (örn.
  0.8.0) canlıya almayı sevmez. Yayınlara `RELEASING` kuralı olarak "önce
  tek tek, her adım local'de test edilmiş" uygula.

## 1. Versiyon bump — hangi dosyalar, tek seferde

`version` **9 manifestte + vendored C kopyalarda + Cargo.lock** tek sürümde.

check_versions.py'nin referans aldığı 9 manifest:

| Dosya | Alan |
|------|------|
| `pyproject.toml` | `version = "..."` |
| `abi/ccharts_abi.h` | `#define CCHARTS_VERSION "..."` |
| `bindings/rust/Cargo.toml` | `version = "..."` |
| `bindings/js/package.json` | `"version": "..."` |
| `bindings/dotnet/src/Ccharts/Ccharts.csproj` | `<Version>...</Version>` |
| `bindings/java/pom.xml` | `<version>...</version>` |
| `bindings/ruby/lib/ccharts/version.rb` | `VERSION = "..."` |
| `bindings/lua/src/ccharts/version.lua` | `VERSION = "..."` |
| `bindings/julia/Project.toml` | `version = "..."` |

Ayrıca elle güncellenmesi gerekenler (check_versions KONTROL ETMEZ):

- `bindings/rust/Cargo.lock` — ccharts `version` satırı.
- `bindings/lua/ccharts-*.rockspec` — dosya adı ve `version` satırı (`3.0.0-1`).
- Vendored C kopyaları: `bindings/rust/vendor/ccharts_abi.h`,
  `bindings/js/vendor/ccharts_abi.h`, `bindings/go/ccharts/ccharts_abi.h`,
  `bindings/ruby/ext/ccharts/vendor/ccharts_abi.h`, `bindings/lua/vendor/ccharts_abi.h`,
  `bindings/julia/vendor/ccharts_abi.h`.
  Bunları **elle değil** `python3 scripts/sync_sources.py` ile tazele
  (abi/ccharts_abi.h'ten kopyalar). CCHARTS_VERSION makrosunun yanı sıra,
  `abi/ccharts_abi.h` içinde "Library version (...)" yorum satırı da sürüm
  string'i içerir — ikisini de güncelle, yoksa sync sonrası grep hâlâ eski
  sürümü bulur.

**Doğrulama (push etmeden önce şart):**

```sh
python3 scripts/check_versions.py            # 9 manifest aynı sürümde olmalı
python3 scripts/sync_sources.py --check      # vendored kaynaklar güncel olmalı
make test                                    # C demo (line + candle)
python3 -m unittest discover -s tests -v     # Python testleri
```

`make test` + `python3 -m unittest` + `check_versions` + `sync_sources --check`
dördü birden geçmeden tag push etme.

## 2. Yayın — tag push'u ne tetikler

```sh
git tag v<version> && git push origin main --tags
```

`publish.yml` (push `v*` tetikler) şunları yapar:

- `check-version` — tag sürümü ile tüm manifestler uyuşmalı.
- `sdist` + `wheels` (cibuildwheel) → PyPI (`publish` job).
- `publish-crates` — `cargo publish --dry-run` önce, sonra `cargo publish`.
- `publish-npm` — `node --test` + `npm publish --provenance`.
- `publish-nuget` — native'ler + `NuGet/login@v1` (trusted publishing) + push.
- `publish-maven` — native'ler + `mvn -Prelease -DskipTests deploy` (Central Portal).
- `publish-gem` — `gem build` + `gem push` (RubyGems).
- `publish-luarocks` — `luarocks upload` (LuaRocks).
- `tag-go-module` — `bindings/go/v<version>` alt tag'ini otomatik pushlar.

Farklı registry'lerin bağımsız versiyonlarını yayınılmatla bu mono-tag akışı
"her şey aynı sürümde" garantisini verir.

## 3. Secret'lar — neler gerekli, hangi amaçla

| Secret | Registry | Not |
|--------|----------|-----|
| `PYPI_API_TOKEN` | PyPI | `publish.yml` `password` olarak; trusted publishing'e geçilebilir |
| `CARGO_REGISTRY_TOKEN` | crates.io | crates.io hesabından, `publish-update` scope'u yetebilir |
| `NPM_TOKEN` | npm | npm'ın önerdiği "granular access token" (publish yetkili) |
| `NUGET_USER` | NuGet | nuget.org **profil adı** (email DEĞİL), `NuGet/login@v1` `user` parametresi |
| `MAVEN_CENTRAL_USERNAME` | Maven | Central Portal "User Token" username |
| `MAVEN_CENTRAL_PASSWORD` | Maven | Central Portal "User Token" password |
| `MAVEN_GPG_PRIVATE_KEY` | Maven | ASCII-armored private key (base64 DEĞİL!) |
| `MAVEN_GPG_PASSPHRASE` | Maven | GPG key passphrase |
| `RUBYGEMS_API_KEY` | RubyGems | `GEM_HOST_API_KEY` olarak `gem push` yetkili API key |
| `LUAROCKS_API_KEY` | LuaRocks | `luarocks upload --api-key` yetkili API key |

**İsim-aslında-adi notu:** Secret isimlerini README'ye yazma — bunlar iç ops
detayı, kullanıcı-yönelik dokümanda sekerek durmamalı. "registry + credential
GitHub secret'ıdır, workflow'da belgelenir." Nokta.

## 4. NuGet — trusted publishing (API key yok)

- nuget.org → avatar → **Trusted Publishing** → policy ekle:
  - Owner `dethrandir`, Repo `ccharts`, **Workflow File** (dosya adı, path değil),
    Environment boş.
  - **Glob Patterns** alanına paket adı: `Ccharts` (boş bırakılamaz).
- Workflow'ta: job'a `permissions: id-token: write`, sonra
  `NuGet/login@v1` (with: `user: ${{ secrets.NUGET_USER }}`), sonra
  `dotnet nuget push --api-key "${{ steps.login.outputs.NUGET_API_KEY }}"`.
- **Dikkat:** İki ayrı NuGet workflow'u var — standalone `publish-nuget.yml`
  ve ana `publish.yml`. İkisi de trusted publishing kullanır. NuGet policy'si
  workflow **dosya adına** bağlıdır; kullanılacak workflow'a göre policy de
  o dosya adını hedeflemeli. `publish.yml` içindeki push artık `NUGET_API_KEY`
  **değil** `NuGet/login` (OIDC) kullanır.
- Push çıktısında `Created ... <url>` (HTTP 201) görürsen paket kabul edilmiş
  demektir. `flatcontainer`/`registration` endpoint'leri hemen 404 verebilir;
  indexing **dakikalar alır**, 404'e aldanma.

## 5. Maven Central — en ağır olanı

- **Site:** `central.sonatype.com` (eski sahibi Sonatype, şimdi Linux
  Foundation; domain tarihsel). Doğru adres generic, resmi.
- **Namespace:** `io.github.<github_username>` formatı (`io.github.dethrandir`)
  genelde otomatik doğrulanır (GitHub kullanıcı adıyla eşleşir). birkaç saat.
- **User Token:** Central Portal → "Generate User Token" → username+password.
- **GPG zorunlu:** Central imzasız/shattle paketlemi reddeder.
  - GPG key üret, **public key'i desteklenen keyserver'a yayınla.**
  - Central'ın desteklediği keyserver'lar: `keyserver.ubuntu.com`,
    `keys.openpgp.org`, `pgp.mit.edu`. (SKS ağı deprecated; ubuntu+mit SKS
    pool'una gider, sync kararsız olabilir — `keys.openpgp.org` en sağlam.)
  - **`MAVEN_GPG_PRIVATE_KEY` ASCII-armored olmalı, base64 DEĞİL.** gpg-armor
    çıktısını (`-----BEGIN PGP PRIVATE KEY BLOCK-----`) secret'a koy.
- `pom.xml` zaten Central Portal'a deploy edecek şekilde hazır:
  `central-publishing-maven-plugin` + `maven-gpg-plugin` + sources/javadoc.
- **Sürüm hatası:** plugin `0.7.0` iken Central API response'undaki yeni
  `warnings` alanını parse edemeyip `UnrecognizedPropertyException` ile
  patlıyordu. Güncel sürüme (`0.11.0`) yükseltmek çözdü.
- **"Deployment ... failed while publishing"** log'u yayını yapılmamış
  demek DEĞİLDİR! Eğer component zaten yayınlandıysa (önceki deployment
  başarılı olduysa) Central gerçek sebebi söyler ve tekrar denersen
  "Component ... already exists" döner — bu paketin **canlıda olduğunu**
  gösterir. Central'ın nihai sebebini her zaman Central Portal →
  Deployments → deployment'a tıklayıp oku (workflow log'undan görünmez).
- GPG public key'i yayınladıktan sonra keyserver pool'unun sync'i dakikalar
  sürebilir; "public key bulunamadı" görürsen hemen pes etme, key'i
  `keys.openpgp.org`'a da yükle ve birkaç dakika bekle.

## 6. Go module — subdirectory tag'i şart ve hassas

- Go binding bir **subdirectory module**'üdür (`bindings/go/go.mod`). Go,
  böyle bir module'ün kimliğini **`bindings/go/v<version>`** prefiksli tag'den
  alır.
- **Kritik tuzak:** Go, `go get .../bindings/go@vX.Y.Z` çağrısında **kök
  `vX.Y.Z` tag'i varsa onu** tercih eder (v0 büyük sürüm). Eğer kök `v0.2.0`
  tag'i Go binding'den **önceki** bir commit'e işaret ediyorsa, Go "module
  ... found, but does not contain package bindings/go" der ve subdir tag'ine
  hiç bakmaz. → sürüm yayınlanırken **kök tag ve Go subdir tag'i aynı**
  (güncel) commit'e işaret etmelidir.
- Ana `publish.yml`'deki `tag-go-module` job'ı `bindings/go/v<version>`'i
  otomatik pushlar. Elle sheket okuyup hata yapmaktansa **doğru akış**:

```sh
git tag v0.2.1                                # kök tag, önce
git push origin main --tags                   # publish.yml koşar, go tag'i otomatik yaratır
```

  Kök `v0.2.0` gibi **tarihî bir kök tag'i taşıma/push etme** — yayını bozar.
  Bunun yerine yeni sürüm (v0.2.1) yayınla.

## 7. Gözlemlenen hatalar ve çözümleri (v0.2.1 yolculuğu)

| Hata | Sebep | Çözüm |
|------|-------|-------|
| `npm error 403 ... Package name too similar to existing package c-charts` | npm name-squatting koruması; `ccharts` unscoped ismi `c-charts` ile çakışıyor | Paketi **scoped** `@dethrandir/ccharts` yayınla (`--access public`) |
| `setup-java` gpg import fail (exit 2) | `MAVEN_GPG_PRIVATE_KEY` base64'dü | ASCII-armored (base64 değil) olarak secret'a koy |
| `central-publishing-maven-plugin:0.7.0` `UnrecognizedPropertyException: warnings` | Eski plugin, Central API yeni `warnings` alanı ekledi | Plugin'i 0.11.0'a yükselt |
| `NuGet push 401 (An API key must be provided in X-NuGet-ApiKey)` | `publish.yml` hâlâ `NUGET_API_KEY` secret'ına güveniyordu; o secret yok | Trusted publishing (`NuGet/login@v1` + `id-token: write`) kullan |
| `NuGet ... flatcontainer 404` push sonrası | NuGet indexing gecikmesi | Push `201 Created` verdiğiyse sorun yok; indexing dakikalar alır |
| `go get ... found, but does not contain package bindings/go` | Kök `v0.2.0` tag'i Go binding'den önceki commit'e işaret ediyor | Kök tag'i güncel sürüme taşıma; yeni sürüm yayınla |
| macOS osx-x64 native build'i saatlerce `queued` kalır | free GitHub hesabında `macos-13` (x64) runner kapasitesi yok denecek kadar az | `natives.yml`'den osx-x64'ü çıkar; sadece arm64 Mac native'i yayınla |

## 8. Standalone per-registry workflow'lar

Ana `publish.yml` dışında, tek bir registry'yi izole test/publish etmek için
standalone workflow'lar var (hepsi `workflow_dispatch` ile tetiklenir):

- `publish-npm.yml` — sadece JS/WASM paketi.
- `publish-crates.yml` — sadece Rust crate.
- `publish-nuget.yml` — sadece C# paketi (native'leri kendisi build eder).
- `publish-maven.yml` — sadece Java paketi.

Bunlar "hepsi birden" mono-tag akışına girmeden tek registry'yi test etmek
için. Not: NuGet/Maven policy'leri (workspace dosya adına bağlı) standalone
ile ana workflow arasında **aynı değildir**; standalone policy'si
`publish-nuget.yml` / `publish-maven.yml` dosya adını hedeflemeli.

Tetikleme: `gh workflow run publish-nuget.yml`.

## 9. Kullanışlı komutlar

```sh
python3 scripts/check_versions.py
python3 scripts/sync_sources.py --check
python3 scripts/gen_golden.py --check
gh secret list
gh run list --workflow=publish.yml --limit 1
gh run watch <run-id> --exit-status
```