# Bevel Programming Language

**Bevel** - язык сочетающий синтаксис C и Python.
Данный язык, лишь эксперимент. Компилятор написан при помощи Qwen AI на чистом C. В данный момент язык работает не полностью. Компилятор не компилирует большинство конструкций.
Язык рассматривался как очень C, но с дополнительными функциями. В Bevel остутствует классическое ООП, вместо него interface, methods, struct. 

Я намерено отказался от некоторых операторов и названий,вернулся к синтаксису C. 

Я увлекся разработкой геометрических ядер, язык C++ для меня показался слишком сложным, а язык C для этого дела мало подходил, поэтому захотелось чего-то своего.

Здесь вы не найдете синтаксис по типу 

```
fn main() -> i32 {
    let a, b = 5;
    return a + b;
}
```

вместо него я решил сделать так:

```
i32 main():
    i32 a, b = 5;
    return a + b;
```
Хорошо это или плохо, решайте сами. Я лишь экспериментировал. Может кому-то понравится данный язык и захочет его развивать дальше. Я буду заниматься им лишь в свободное время.

Отсутствует:
- import

Не доработано: 
- Generics
- что-то еще, но я уже не помню.

Язык компилируется в C код. Поэтому после компиляции, нужно отдельно компилировать его через компилятор языка C.

## Как запустить
Нужно лишь выполнить следующие команды:
```
$ git clone https://github.com/KulaginSN/bevel-lang
$ cd bevel-lang
$ make
```

Если вам нужно очистить и пересобрать проект выполните следующую команду:
```
$ make clean && make && clear
```

## Примеры
В папке tests/ вы найдете примеры синтаксиса, может быть он вам понравится.

Функции:

```
i32 add(i32 a, i32 b):
    return a + b;

i32 main():
    return add(10, 20);
```

Циклы:
```
i32 main():
    i32 sum = 0;
    for (i32 i = 0; i < 5; i = i + 1):
        sum = sum + i;
    return sum;

---

int main():
    i32 i = 0;
    i32 sum = 0;
    
    while (i < 10):
        i = i + 1;
        if (i == 5):
            continue;
        if (i == 8):
            break;
        sum = sum + i;
        
    return sum;
```
Структуры и перечисления:
```
struct Point:
    i32 x;
    i32 y;

i32 main():
    Point p;
    *Point ptr = &p;
    ptr.x = 100;
    return p.x;

---

enum i32 Status:
    Ok = 0;
    Err = 1;

i32 main():
    i32 s = Status.Ok;
    return s;
```

Интерфейсы и методы:
```
interface Shape:
    f64 area(self);

struct Circle:
    f64 radius;

methods Circle:
    f64 area(self):
        return 3.14 * self.radius * self.radius;

i32 main():
    Circle c;
    c.radius = 5.0;
    Shape s = c;
    f64 a = s.area();
    if (a > 78.0):
        return 1;
    return 0;

```

Дженерики:
```
generic(T)
T max_val(T a, T b):
    if (a > b):
        return a;
    return b;

i32 main():
    i32 x = max_val(i32, 5, 10);
    if (x == 10):
        return 1;
    return 0;

---

interface Printable:
    void print(self);

generic(T) where T: Printable
void show(T item):
    item.print();

struct Number:
    i32 value;

methods Number:
    void print(self):
        return;

i32 main():
    Number n;
    n.value = 42;
    show(n);
    return 1;

---

generic(T, E)
struct Result:
    T value;
    E error;
    bool is_ok;

i32 main():
    Result(f64, string) res1;
    Result(i32, i32) res2;
    
    res1.value = 3.14;
    res1.is_ok = true;
    
    res2.value = 42;
    res2.error = 0;
    res2.is_ok = true;
    
    if (res1.is_ok && res2.is_ok):
        return 1;
        
    return 0;
```

Для того чтобы  скомпилировать какой-нибудь код на bevel и запустить его надо вписать следующие команды:

```
cd bevel-lang
./bevel_test [ИМЯ_ФАЙЛА.bv]
gcc [ИМЯ_ФАЙЛА].c -o program
./program
```

Тут есть сообщения о дебагах, так что не обращайте внимание.

## Планы разработки
1. Исправления ошибок
2. Доведение компилятора до рабочего состояния.
3. Пакетный менеджер bforge (как cargo).

## Состояние проекта

Наверно проект еще даже не в версии 0.1
Требуется многое переработать и добавить функционал.
Если вы хотите внести вклад, пишите, я добавлю вас в контеребьютеры.
