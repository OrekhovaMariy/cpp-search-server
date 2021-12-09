// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main() {
	vector<string> a;
	for (int x = 1; x < 1000; ++x) {
		string b = to_string(x);
		a.push_back(b);
	}
	int count = 0;
	for (int i = 0; i < a.size(); ++i) {
		for (char z : a[i]) {
				if (z == '3') {
					count += 1;
				}
			}
		}
	cout << count << endl;
	return 0;
	}
// Закомитьте изменения и отправьте их в свой репозиторий.
