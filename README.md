# ระบบจัดสรรสินค้าคอมพิวเตอร์ (C)

เว็บจัดการสต็อกสินค้าไอที แยกตามหมวด แจ้งเตือน **ใกล้หมด** และ **สินค้าหมด**
Backend เขียนด้วยภาษา C ล้วน (ไม่มี dependency) · Frontend เป็น HTML/JS ไฟล์เดียว

## รัน
```bash
make && ./server 8080
```
เปิด http://localhost:8080 (ข้อมูลเก็บใน `data.tsv`)

## ขึ้น GitHub
```bash
git init && git add . && git commit -m "stock system in C"
git branch -M main
git remote add origin https://github.com/<ชื่อคุณ>/<repo>.git
git push -u origin main
```
> GitHub Pages รันโปรแกรม C ไม่ได้ (โฮสต์ได้เฉพาะไฟล์ static) — ให้ clone ไปรันบนเครื่อง/VPS/Render/Railway หรือใช้ Docker

## API
| Method | Path | พารามิเตอร์ |
|---|---|---|
| GET | /api/items | – |
| POST | /api/items | cat, name, qty, min |
| POST | /api/adjust | id, delta (+รับเข้า / −จ่ายออก) |
| POST | /api/delete | id |
