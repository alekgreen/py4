package main

import "fmt"

const iterations = 5000000

func work(limit int) int {
	i := 0
	a := 1
	b := 2
	total := 0

	for i < limit {
		a = a + b
		for a > 1000000 {
			a = a - 1000000
		}

		b = b + total + 3
		for b > 1000000 {
			b = b - 1000000
		}

		total = total + (a * 3) + (b * 2) + i
		for total > 1000000 {
			total = total - 1000000
		}

		i = i + 1
	}

	return total
}

func main() {
	fmt.Println(work(iterations))
}
