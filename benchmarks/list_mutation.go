package main

import "fmt"

const listSize = 2000
const rounds = 1000

func wrap97(value int) int {
	for value >= 97 {
		value -= 97
	}
	return value
}

func wrap5(value int) int {
	for value >= 5 {
		value -= 5
	}
	return value
}

func work(size int, rounds int) int {
	xs := make([]int, 0, size)
	total := 0

	for i := 0; i < size; i++ {
		xs = append(xs, wrap97(i))
	}

	for r := 0; r < rounds; r++ {
		for j := 0; j < len(xs); j++ {
			xs[j] = xs[j] + wrap5(j+r)
			total += xs[j]
			for total > 1000000 {
				total -= 1000000
			}
		}
	}

	return total
}

func main() {
	fmt.Println(work(listSize, rounds))
}
