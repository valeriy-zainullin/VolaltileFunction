__attribute__((optnone)) int not_optimized() {
	return 3;
}

int main() {
	int a[2];
	if (sizeof(a) / sizeof(int) == 2) {
		return 1;
	}
	int b = 2;
	if (((volatile int) b) == 2) {
		return 2;
	}
	return 3;
}