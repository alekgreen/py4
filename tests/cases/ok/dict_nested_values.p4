def main() -> None:
    team_a: dict[str, int] = {"wins": 3, "losses": 1}
    team_b: dict[str, int] = {"wins": 2, "losses": 2}
    empty: dict[str, int] = {"wins": 0, "losses": 0}
    scores: dict[str, dict[str, int]] = {"team_a": team_a, "team_b": team_b}

    print(scores)
    print(scores["team_a"]["wins"])
    print(scores.get_or("team_c", empty))
    print(scores.values())
    print(scores.items())

    for (name, record) in scores.items():
        print(name)
        print(record["losses"])
