# ft_irc Makefile

NAME := ircserv
CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98 -pedantic -g #-fsanitize=address
LDFLAGS :=

SRC_DIR := src
OBJ_DIR := build
INC_DIR := include

SRCS := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/Server.cpp \
	$(SRC_DIR)/Poller.cpp \
	$(SRC_DIR)/Client.cpp \
	$(SRC_DIR)/Channel.cpp \
	$(SRC_DIR)/Parser.cpp \
	$(SRC_DIR)/Router.cpp \
	$(SRC_DIR)/Command.cpp \
	$(SRC_DIR)/Utils.cpp

OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re