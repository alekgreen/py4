import inventory_lib

def print_summary(stock: list[int], incoming: list[int], weekly_demand: list[int], unit_costs: list[float], sell_prices: list[float], target_weeks: int) -> None:
    total_units: int = total_int(stock) + total_int(incoming)
    low_items: int = shortage_count(stock, incoming, weekly_demand, target_weeks)
    ((best_idx, best_margin_value), current_value): ((int,float),float) = overview(stock, incoming, weekly_demand, unit_costs, sell_prices, target_weeks)

    print("summary")
    print(total_units)
    print(low_items)
    print((best_idx, best_margin_value))
    print(current_value)

def print_item_lines(codes: list[char], stock: list[int], incoming: list[int], weekly_demand: list[int], unit_costs: list[float], sell_prices: list[float], target_weeks: int) -> None:
    print("items")
    for i in range(len(codes)):
        cover: float = weeks_of_cover(stock[i], incoming[i], weekly_demand[i])
        reorder: int = reorder_quantity(stock[i], incoming[i], weekly_demand[i], target_weeks)
        margin: float = gross_margin(sell_prices[i], unit_costs[i])

        print(codes[i])
        print(cover)
        print(reorder)
        print(margin)
        print(item_signal(cover))

def main() -> None:
    target_weeks: int = 4
    codes: list[char] = ['A', 'B', 'C', 'D', 'E']
    stock: list[int] = [12, 4, 30, 9, 18]
    incoming: list[int] = [0, 8, 0, 3, 0]
    weekly_demand: list[int] = [5, 6, 7, 2, 4]
    unit_costs: list[float] = [3.5, 8.0, 1.2, 12.5, 5.0]
    sell_prices: list[float] = [5.0, 12.5, 2.0, 19.0, 7.5]

    print_summary(stock, incoming, weekly_demand, unit_costs, sell_prices, target_weeks)
    print_item_lines(codes, stock, incoming, weekly_demand, unit_costs, sell_prices, target_weeks)
