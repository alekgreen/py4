def total_int(xs: list[int]) -> int:
    total: int = 0
    for x in xs:
        total = total + x
    return total

def inventory_value(stock: list[int], incoming: list[int], unit_costs: list[float]) -> float:
    total: float = 0.0
    for i in range(len(stock)):
        total = total + (stock[i] + incoming[i]) * unit_costs[i]
    return total

def weeks_of_cover(stock: int, incoming: int, weekly_demand: int) -> float:
    if weekly_demand == 0:
        return 0.0
    return (stock + incoming) / weekly_demand

def reorder_quantity(stock: int, incoming: int, weekly_demand: int, target_weeks: int) -> int:
    target_units: int = weekly_demand * target_weeks
    available: int = stock + incoming
    if available >= target_units:
        return 0
    return target_units - available

def gross_margin(sell_price: float, unit_cost: float) -> float:
    return sell_price - unit_cost

def best_margin(sell_prices: list[float], unit_costs: list[float]) -> (int,float):
    best_idx: int = 0
    best_value: float = gross_margin(sell_prices[0], unit_costs[0])

    for i in range(1, len(sell_prices)):
        margin: float = gross_margin(sell_prices[i], unit_costs[i])
        if margin > best_value:
            best_idx = i
            best_value = margin

    return (best_idx, best_value)

def shortage_count(stock: list[int], incoming: list[int], weekly_demand: list[int], target_weeks: int) -> int:
    count: int = 0
    for i in range(len(stock)):
        if reorder_quantity(stock[i], incoming[i], weekly_demand[i], target_weeks) > 0:
            count = count + 1
    return count

def item_signal(cover: float) -> str:
    if cover < 2:
        return "urgent"
    elif cover < 4:
        return "watch"
    else:
        return "stable"

def overview(stock: list[int], incoming: list[int], weekly_demand: list[int], unit_costs: list[float], sell_prices: list[float], target_weeks: int) -> ((int,float),float):
    return (best_margin(sell_prices, unit_costs), inventory_value(stock, incoming, unit_costs))
