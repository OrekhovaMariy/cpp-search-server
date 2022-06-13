**SearchServer**

Поисковая система документов, работа которой похожа на работу поисковика.

Сначала происходит наполнение системы документами. При заполнении системы не учитываются стоп-слова, например, предлоги и союзы не играют роли при происке документов. Поиск по массиву данных происходит с учетом минус-слов, то есть, если в документе встречается минус-слово, то данный документ не будет выведен в результат поиска. Вывод результата поиска происходит по TF-IDF.

Написана система с использованием стандарта ISO C++17 (/std:c++17).

Использованы функции (в том числе лямбда-функция), шаблоны, структуры, классы и их конструкторы, алгоритмы стандартных библиотек. Применена область видимости переменных. К коду написаны юнит-тесты. Произведена обработка ошибок и исключений. Код оптимизирован с применением макросов и разбит на несколько файлов.
