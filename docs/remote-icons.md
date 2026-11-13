# Remote icons

Rclone Explorer shows every remote with a colour-coded tile for its storage type. You can replace any tile with your own picture.

## Included logos

The download already contains logos for: `azureblob`, `b2`, `box`, `drime`, `drive`, `dropbox`, `fichier`, `filelu`, `filen`, `filescom`, `gcs`, `gofile`, `gphotos`, `hidrive`, `iclouddrive`, `internxt`, `jottacloud`, `koofr`, `local` (a folder on this PC), `mailru`, `mega`, `onedrive`, `opendrive`, `pcloud`, `pikpak`, `pixeldrain`, `premiumizeme`, `protondrive`, `putio`, `s3`, `seafile`, `storj`, `tardigrade`, `ulozto`, `zoho`. They are ordinary files in `icons\remotes` and can be replaced or deleted like your own.

## How to add your own icon

1. Open (or create) the logo folder:
   - **Windows:** `icons\remotes` next to `Rclone Explorer.exe`
   - **Linux:** `~/.local/share/rclone-browser/rclone-browser/icons/remotes/`, or `icons/remotes/` next to the AppImage in portable mode
2. Save the picture as `<type>.png`, `<type>.svg` or `<type>.ico`, using the **file name from the table below** (all lower case). Example: `icons\remotes\drive.png` for Google Drive.
3. Use a square image. Recommended: SVG, or PNG at 256 × 256 px with a transparent background.
4. Restart Rclone Explorer, or right-click a remote in the sidebar and choose **Reload remotes**.

If one file exists in several formats, PNG is used first, then SVG, then ICO. The icon applies to every remote of that type. To find a remote's type, look under its name on the Home page, or run `rclone listremotes --long`.

**Suggested priority:** the *Cloud drive*, *Photos & media* and *Object storage* services are the ones most worth giving a recognisable icon. The *Local / virtual* types (alias, crypt, union…) are wrappers around other remotes and usually look fine with the built-in tile.

The list covers all 69 storage types in rclone 1.75.

## Cloud drive

| File name | Service | Built-in tile |
|---|---|---|
| `box.png` | Box | cloud, `#1A73C7` |
| `drime.png` | Drime | cloud, `#7C3AED` |
| `drive.png` | Google Drive | cloud, `#1E8E3E` |
| `dropbox.png` | Dropbox | cloud, `#2D5BE3` |
| `fichier.png` | 1Fichier | cloud, `#E65A28` |
| `filefabric.png` | Enterprise File Fabric | cloud, `#4A5D7A` |
| `filelu.png` | FileLu Cloud Storage | cloud, `#2E7DD7` |
| `filen.png` | Filen | cloud, `#1F2937` |
| `filescom.png` | Files.com | cloud, `#2563EB` |
| `gofile.png` | Gofile | cloud, `#3B82F6` |
| `hidrive.png` | HiDrive | cloud, `#E2001A` |
| `huaweidrive.png` | Huawei Drive | cloud, `#CF0A2C` |
| `iclouddrive.png` | iCloud Drive and Photos | cloud, `#3A8EE6` |
| `internxt.png` | Internxt Drive | cloud, `#0066FF` |
| `jottacloud.png` | Jottacloud | cloud, `#6A4FB8` |
| `koofr.png` | Koofr, Digi Storage | cloud, `#2F9E6E` |
| `linkbox.png` | Linkbox | cloud, `#3BA7C9` |
| `mailru.png` | Mail.ru Cloud | cloud, `#1D6FD8` |
| `mega.png` | Mega | cloud, `#D9272E` |
| `onedrive.png` | Microsoft OneDrive | cloud, `#0F6CBD` |
| `opendrive.png` | OpenDrive | cloud, `#3E82C4` |
| `pcloud.png` | Pcloud | cloud, `#139C9C` |
| `pikpak.png` | PikPak | cloud, `#3D7BE0` |
| `pixeldrain.png` | Pixeldrain Filesystem | cloud, `#4F8A3C` |
| `premiumizeme.png` | premiumize.me | cloud, `#E0632A` |
| `protondrive.png` | Proton Drive | cloud, `#6D4AFF` |
| `putio.png` | Put.io | cloud, `#E3A72A` |
| `quatrix.png` | Quatrix by Maytech | cloud, `#0E7490` |
| `seafile.png` | seafile | cloud, `#E06C16` |
| `shade.png` | Shade FS | cloud, `#525252` |
| `sharefile.png` | Citrix Sharefile | cloud, `#4C6A8E` |
| `sugarsync.png` | Sugarsync | cloud, `#0A84C6` |
| `ulozto.png` | Uloz.to | cloud, `#E5007E` |
| `yandex.png` | Yandex Disk | cloud, `#E0A100` |
| `zoho.png` | Zoho | cloud, `#C8312B` |

## Photos & media

| File name | Service | Built-in tile |
|---|---|---|
| `cloudinary.png` | Cloudinary | photos, `#3448C5` |
| `gphotos.png` | Google Photos | photos, `#C2185B` |
| `imagekit.png` | ImageKit.io | photos, `#0450D5` |

## Object storage

| File name | Service | Built-in tile |
|---|---|---|
| `azureblob.png` | Microsoft Azure Blob Storage | bucket, `#0F78D4` |
| `azurefiles.png` | Microsoft Azure Files | bucket, `#0F78D4` |
| `b2.png` | Backblaze B2 | bucket, `#C8312B` |
| `gcs.png` | Google Cloud Storage (this is not Google Drive) | bucket, `#3367D6` |
| `netstorage.png` | Akamai NetStorage | bucket, `#0096D6` |
| `oos.png` | Oracle Cloud Infrastructure Object Storage | bucket, `#C74634` |
| `qingstor.png` | QingCloud Object Storage | bucket, `#2D8CF0` |
| `s3.png` | Amazon S3 and S3-compatible storage (AWS, Cloudflare R2, Wasabi, MinIO and more) | bucket, `#D9822B` |
| `sia.png` | Sia Decentralized Cloud | bucket, `#1ED660` |
| `storj.png` | Storj Decentralized Cloud Storage | bucket, `#2683FF` |
| `swift.png` | OpenStack Swift (Rackspace, OVH and more) | bucket, `#B85C1E` |
| `tardigrade.png` | Storj Decentralized Cloud Storage | bucket, `#2683FF` |

## Server / protocol

| File name | Service | Built-in tile |
|---|---|---|
| `ftp.png` | FTP | server, `#3B7E5B` |
| `hdfs.png` | Hadoop distributed file system | server, `#5E6B78` |
| `http.png` | HTTP | globe, `#4E6A92` |
| `sftp.png` | SSH/SFTP | server, `#0E7C86` |
| `smb.png` | SMB / CIFS | server, `#4E6A92` |
| `webdav.png` | WebDAV | server, `#7553C0` |

## Archive

| File name | Service | Built-in tile |
|---|---|---|
| `doi.png` | DOI datasets | archive, `#555D66` |
| `internetarchive.png` | Internet Archive | archive, `#555D66` |

## Local / virtual

| File name | Service | Built-in tile |
|---|---|---|
| `alias.png` | Alias for an existing remote | folder, `#C98B1E` |
| `archive.png` | Read archives | folder, `#C98B1E` |
| `cache.png` | Cache a remote | folder, `#C98B1E` |
| `chunker.png` | Transparently chunk/split large files | folder, `#C98B1E` |
| `combine.png` | Combine several remotes into one | folder, `#C98B1E` |
| `compress.png` | Compress a remote | folder, `#C98B1E` |
| `crypt.png` | Encrypt/Decrypt a remote | lock, `#5B5FC7` |
| `hasher.png` | Better checksums for other remotes | folder, `#C98B1E` |
| `local.png` | Local Disk | drive, `#5E6B78` |
| `memory.png` | In memory object storage system. | drive, `#5E6B78` |
| `union.png` | Union merges the contents of several upstream fs | folder, `#C98B1E` |

## Types not listed

Remote types added in newer rclone versions get a cloud tile in a colour derived from their name. An icon file named after the type works for them too.
