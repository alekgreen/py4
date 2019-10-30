def main() -> None:
    scores: dict[str, dict[str, int]] = {
        "team_a": {"wins": 3, "losses": 1},
        "team_b": {"wins": 2, "losses": 2}
    }

    print(scores)
    print(scores["team_a"]["wins"])
    print(scores.get_or("team_c", {"wins": 0, "losses": 0}))
    print(scores.values())
    print(scores.items())

    for (name, record) in scores.items():
        print(name)
        print(record["losses"])
