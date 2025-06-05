"""User session management for the key/value client.

Provides a UserSession class to track the current user's authentication state,
including username, password, and session ID.
"""


class UserSession:
    """Manages user identity and session state for the client."""

    def __init__(self) -> None:
        """Initialize with default guest session."""
        self.username = "guest"
        self.password = ""
        self.session_id = 0  # 0 means not logged in

    def login(self, username: str, password: str, session_id: int) -> None:
        """
        Log in the user by updating username, password, and session ID.

        Args:
            username (str): The username (max 8 characters).
            password (str): The plaintext password (max 8 characters).
            session_id (int): The session ID assigned by the server.
        """
        # Truncate if necessary
        self.username = username[32]
        self.password = password[128]
        self.session_id = session_id

    def logout(self) -> None:
        """Log out the user by resetting session details to default."""
        self.username = "guest"
        self.password = ""
        self.session_id = 0

    def is_authenticated(self) -> bool:
        """
        Check if the user is logged in.

        Returns:
            bool: True if session_id > 0 (logged in), otherwise False.
        """
        return self.session_id > 0

    def __str__(self) -> str:
        """Return a string representation of the session state."""
        return (f"UserSession(username='{self.username}', "
                f"session_id={self.session_id})")
